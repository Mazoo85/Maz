#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace maz::audio {

// ---- DSP effects + mix buses -------------------------------------------------------------------
// Godot's AudioServer routes every voice through a BUS, and each bus carries an ordered chain of
// EFFECTS (filter, delay, reverb, ...). Maz's mixer was a flat sum with no per-bus processing. This
// adds the reusable, testable core: RBJ-cookbook biquad filters, a feedback delay (echo), and a `Bus`
// that chains effects in series with an output gain. Pure per-sample math — no device, no threads — so
// it unit-tests exactly and drives a deterministic offline waveform golden, and a real-time mixer can
// consume it unchanged (process one sample, or a whole buffer, through the chain).

// Second-order IIR "biquad" — the standard filter behind Godot's AudioEffectFilter. Build one with a
// cutoff + resonance (Q) via the factories, then stream samples through `process` (transposed direct
// form II: one multiply-add per coefficient, and numerically well-behaved). Coefficients are stored
// already normalized by a0, so `process` needs no division.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f; // feed-forward (numerator) coefficients
    float a1 = 0.0f, a2 = 0.0f;            // feed-back (denominator) coefficients (a0 normalized to 1)
    float z1 = 0.0f, z2 = 0.0f;            // delay-line state

    void reset() {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // RBJ audio-EQ-cookbook designs. `q` is resonance (0.707 = Butterworth / maximally flat).
    static Biquad lowpass(float cutoffHz, float q, float sampleRate) {
        float cw, alpha;
        commonTerms(cutoffHz, q, sampleRate, cw, alpha);
        const float a0 = 1.0f + alpha;
        Biquad f;
        f.b0 = (1.0f - cw) * 0.5f / a0;
        f.b1 = (1.0f - cw) / a0;
        f.b2 = f.b0;
        f.a1 = (-2.0f * cw) / a0;
        f.a2 = (1.0f - alpha) / a0;
        return f;
    }

    static Biquad highpass(float cutoffHz, float q, float sampleRate) {
        float cw, alpha;
        commonTerms(cutoffHz, q, sampleRate, cw, alpha);
        const float a0 = 1.0f + alpha;
        Biquad f;
        f.b0 = (1.0f + cw) * 0.5f / a0;
        f.b1 = -(1.0f + cw) / a0;
        f.b2 = f.b0;
        f.a1 = (-2.0f * cw) / a0;
        f.a2 = (1.0f - alpha) / a0;
        return f;
    }

    // Constant 0 dB peak-gain band-pass.
    static Biquad bandpass(float cutoffHz, float q, float sampleRate) {
        float cw, alpha;
        commonTerms(cutoffHz, q, sampleRate, cw, alpha);
        const float a0 = 1.0f + alpha;
        Biquad f;
        f.b0 = alpha / a0;
        f.b1 = 0.0f;
        f.b2 = -alpha / a0;
        f.a1 = (-2.0f * cw) / a0;
        f.a2 = (1.0f - alpha) / a0;
        return f;
    }

private:
    static void commonTerms(float cutoffHz, float q, float sampleRate, float& cosW0, float& alpha) {
        const float pi = 3.14159265358979f;
        const float safeQ = q < 1e-4f ? 1e-4f : q;
        const float w0 = 2.0f * pi * cutoffHz / sampleRate;
        cosW0 = std::cos(w0);
        alpha = std::sin(w0) / (2.0f * safeQ);
    }
};

// A feedback delay (echo) — Godot's AudioEffectDelay tap. A ring buffer `delaySamples` long; each
// output = dry input + `wet` * the delayed signal, and the delayed signal is fed back at `feedback`
// so echoes repeat and decay. feedback in [0,1) stays stable.
struct Delay {
    std::vector<float> line;
    std::size_t idx = 0;
    float feedback = 0.4f;
    float wet = 0.5f;

    void configure(int delaySamples, float fb, float wetMix) {
        line.assign(static_cast<std::size_t>(std::max(1, delaySamples)), 0.0f);
        idx = 0;
        feedback = fb;
        wet = wetMix;
    }

    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        idx = 0;
    }

    float process(float x) {
        if (line.empty()) {
            return x;
        }
        const float echo = line[idx];
        const float out = x + wet * echo;
        line[idx] = x + echo * feedback; // store dry + decayed echo for the next lap
        idx = idx + 1 >= line.size() ? 0 : idx + 1;
        return out;
    }
};

// ---- Reverb (Schroeder / Freeverb) -------------------------------------------------------------
// A feedback COMB filter — the resonant building block of a Schroeder reverb. Outputs the delayed
// sample and feeds it back through a one-pole low-pass (the `damp` control), so high frequencies decay
// faster than lows, as in a real room.
struct Comb {
    std::vector<float> line;
    std::size_t idx = 0;
    float feedback = 0.84f;
    float damp = 0.2f;
    float store = 0.0f;

    void configure(int delaySamples, float fb, float damping) {
        line.assign(static_cast<std::size_t>(std::max(1, delaySamples)), 0.0f);
        idx = 0;
        feedback = fb;
        damp = damping;
        store = 0.0f;
    }
    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        idx = 0;
        store = 0.0f;
    }
    float process(float x) {
        if (line.empty()) {
            return x;
        }
        const float y = line[idx];
        store = y * (1.0f - damp) + store * damp; // low-pass inside the feedback loop
        line[idx] = x + store * feedback;
        idx = idx + 1 >= line.size() ? 0 : idx + 1;
        return y;
    }
};

// A Schroeder ALLPASS filter — flat magnitude response but phase-smearing (diffusion), the second
// stage of a Schroeder reverb that turns the comb resonances into a smooth tail.
struct Allpass {
    std::vector<float> line;
    std::size_t idx = 0;
    float feedback = 0.5f;

    void configure(int delaySamples, float fb) {
        line.assign(static_cast<std::size_t>(std::max(1, delaySamples)), 0.0f);
        idx = 0;
        feedback = fb;
    }
    void reset() {
        std::fill(line.begin(), line.end(), 0.0f);
        idx = 0;
    }
    float process(float x) {
        if (line.empty()) {
            return x;
        }
        const float buf = line[idx];
        const float y = -x + buf;
        line[idx] = x + buf * feedback;
        idx = idx + 1 >= line.size() ? 0 : idx + 1;
        return y;
    }
};

// Schroeder / Freeverb-style reverb: four parallel comb filters (mutually-detuned delay lengths) summed,
// then two series allpasses for diffusion, mixed against the dry signal — Godot's AudioEffectReverb.
struct Reverb {
    std::array<Comb, 4> combs;
    std::array<Allpass, 2> allpasses;
    float wet = 0.3f, dry = 0.7f;

    void configure(float sampleRate, float roomSize = 0.84f, float damp = 0.2f, float wetMix = 0.3f) {
        const int combLen[4] = {1557, 1617, 1491, 1422}; // Freeverb tunings (samples @ 44.1 kHz)
        for (int i = 0; i < 4; ++i) {
            combs[static_cast<std::size_t>(i)].configure(scale(combLen[i], sampleRate), roomSize, damp);
        }
        const int apLen[2] = {225, 341};
        for (int i = 0; i < 2; ++i) {
            allpasses[static_cast<std::size_t>(i)].configure(scale(apLen[i], sampleRate), 0.5f);
        }
        wet = wetMix;
        dry = 1.0f - wetMix;
    }
    void reset() {
        for (Comb& c : combs) c.reset();
        for (Allpass& a : allpasses) a.reset();
    }
    float process(float x) {
        float acc = 0.0f;
        for (Comb& c : combs) {
            acc += c.process(x);
        }
        acc *= 0.25f;
        for (Allpass& a : allpasses) {
            acc = a.process(acc);
        }
        return dry * x + wet * acc;
    }
    static int scale(int n, float sampleRate) {
        return std::max(1, static_cast<int>(static_cast<float>(n) * sampleRate / 44100.0f));
    }
};

// ---- Waveshaper distortion + dynamic-range compressor ------------------------------------------
// Soft-clipping waveshaper (tanh): `drive` pushes the signal into the curve, adding harmonics and
// rounding peaks. Normalized so a full-scale (±1) input stays full-scale — Godot's AudioEffectDistortion.
struct Distortion {
    float drive = 2.0f;

    float process(float x) const {
        const float d = drive < 1e-3f ? 1e-3f : drive;
        return std::tanh(d * x) / std::tanh(d);
    }
    void reset() {}
};

// A peak-envelope compressor: follows the signal level (fast attack, slow release) and, above
// `threshold`, reduces gain toward `ratio`:1 so loud peaks are tamed and the mix stays even — Godot's
// AudioEffectCompressor. attack/release are per-sample smoothing coefficients in (0,1].
struct Compressor {
    float threshold = 0.5f;
    float ratio = 4.0f;
    float attack = 0.01f;
    float release = 0.001f;
    float env = 0.0f;

    void reset() { env = 0.0f; }
    float process(float x) {
        const float a = std::fabs(x);
        env += (a > env ? attack : release) * (a - env); // peak follower
        float gain = 1.0f;
        if (env > threshold && env > 1e-6f) {
            const float compressed = threshold + (env - threshold) / ratio;
            gain = compressed / env;
        }
        return x * gain;
    }
};

// ---- Effect chain + bus ------------------------------------------------------------------------
// A uniform interface so heterogeneous effects can be chained on a bus.
struct Effect {
    virtual ~Effect() = default;
    virtual float process(float x) = 0;
    virtual void reset() {}
};

struct BiquadEffect : Effect {
    Biquad filter;
    explicit BiquadEffect(const Biquad& f) : filter(f) {}
    float process(float x) override { return filter.process(x); }
    void reset() override { filter.reset(); }
};

struct DelayEffect : Effect {
    Delay delay;
    explicit DelayEffect(const Delay& d) : delay(d) {}
    float process(float x) override { return delay.process(x); }
    void reset() override { delay.reset(); }
};

struct ReverbEffect : Effect {
    Reverb reverb;
    explicit ReverbEffect(const Reverb& r) : reverb(r) {}
    float process(float x) override { return reverb.process(x); }
    void reset() override { reverb.reset(); }
};

struct DistortionEffect : Effect {
    Distortion dist;
    explicit DistortionEffect(const Distortion& d) : dist(d) {}
    float process(float x) override { return dist.process(x); }
    void reset() override { dist.reset(); }
};

struct CompressorEffect : Effect {
    Compressor comp;
    explicit CompressorEffect(const Compressor& c) : comp(c) {}
    float process(float x) override { return comp.process(x); }
    void reset() override { comp.reset(); }
};

// A mix bus: an ordered effect chain plus an output gain, like a Godot audio bus. Feed one sample
// through `process`, or a whole buffer through `processBuffer`, and the signal runs each effect in
// series then scales by `gain`. An empty chain is a pass-through (times gain).
struct Bus {
    std::vector<std::unique_ptr<Effect>> effects;
    float gain = 1.0f;

    Bus& add(std::unique_ptr<Effect> e) {
        effects.push_back(std::move(e));
        return *this;
    }

    float process(float x) {
        for (auto& e : effects) {
            x = e->process(x);
        }
        return x * gain;
    }

    void processBuffer(std::vector<float>& samples) {
        for (float& s : samples) {
            s = process(s);
        }
    }

    void reset() {
        for (auto& e : effects) {
            e->reset();
        }
    }
};

} // namespace maz::audio
