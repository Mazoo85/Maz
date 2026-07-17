#include "maz/audio/Effects.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

namespace {
float dbToLin(float db) {
    return std::pow(10.0f, db / 20.0f);
}
float linToDb(float lin) {
    return 20.0f * std::log10(std::max(lin, 1e-9f));
}
} // namespace

// ---- Delay ------------------------------------------------------------------

void Delay::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
}

void Delay::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Size the delay line for up to 2 s; the tap moves within it as timeMs_ changes.
    const int maxSize = sampleRate * 2;
    if (size_ != maxSize) {
        size_ = maxSize;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        write_ = 0;
    }
    int tap = static_cast<int>(timeMs_ * 0.001f * static_cast<float>(sampleRate));
    tap = std::clamp(tap, 1, size_ - 1);
    const float fb = std::clamp(feedback_, 0.0f, 0.95f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const int r = (write_ - tap + size_) % size_;
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        const float wetL = bufL_[static_cast<size_t>(r)];
        const float wetR = bufR_[static_cast<size_t>(r)];
        bufL_[static_cast<size_t>(write_)] = dryL + wetL * fb;
        bufR_[static_cast<size_t>(write_)] = dryR + wetR * fb;
        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
        write_ = (write_ + 1) % size_;
    }
}

// ---- Distortion -------------------------------------------------------------

void Distortion::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float drive = std::max(drive_, 1.0f);
    const float norm = 1.0f / std::tanh(drive); // keep unity-ish level across drive
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    const int n = frames * 2;
    for (int i = 0; i < n; ++i) {
        const float dry = stereo[i];
        const float wet = std::tanh(dry * drive) * norm;
        stereo[i] = dry * (1.0f - mix) + wet * mix;
    }
}

// ---- Chorus -----------------------------------------------------------------

void Chorus::reset() {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
}

void Chorus::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const int maxSize = sampleRate / 20; // up to 50 ms of delay line
    if (size_ != maxSize) {
        size_ = maxSize;
        bufL_.assign(static_cast<size_t>(size_), 0.0f);
        bufR_.assign(static_cast<size_t>(size_), 0.0f);
        write_ = 0;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double phaseInc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float baseMs = 12.0f;
    const float baseSamp = baseMs * 0.001f * static_cast<float>(sampleRate);
    const float depthSamp = std::clamp(depthMs_, 0.0f, 20.0f) * 0.001f * static_cast<float>(sampleRate);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    auto readAt = [&](const std::vector<float>& buf, float delay) {
        float rp = static_cast<float>(write_) - delay;
        while (rp < 0.0f) {
            rp += static_cast<float>(size_);
        }
        const int i0 = static_cast<int>(rp) % size_;
        const int i1 = (i0 + 1) % size_;
        const float frac = rp - std::floor(rp);
        return buf[static_cast<size_t>(i0)] * (1.0f - frac) + buf[static_cast<size_t>(i1)] * frac;
    };

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        bufL_[static_cast<size_t>(write_)] = dryL;
        bufR_[static_cast<size_t>(write_)] = dryR;

        const float modL = static_cast<float>(std::sin(phase_ * kTwoPi));
        const float modR = static_cast<float>(std::sin((phase_ + 0.25) * kTwoPi)); // quadrature
        const float wetL = readAt(bufL_, baseSamp + depthSamp * modL);
        const float wetR = readAt(bufR_, baseSamp + depthSamp * modR);

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;

        write_ = (write_ + 1) % size_;
        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- ParametricEQ -----------------------------------------------------------

void ParametricEQ::setLowGain(float db) {
    lowDb_ = db;
    dirty_ = true;
}
void ParametricEQ::setMid(float freq, float q, float db) {
    midFreq_ = freq;
    midQ_ = q;
    midDb_ = db;
    dirty_ = true;
}
void ParametricEQ::setHighGain(float db) {
    highDb_ = db;
    dirty_ = true;
}

void ParametricEQ::reset() {
    lowL_.reset();
    midL_.reset();
    highL_.reset();
    lowR_.reset();
    midR_.reset();
    highR_.reset();
}

void ParametricEQ::recompute(int sampleRate) {
    lowL_.setShelf(120.0f, lowDb_, sampleRate, false);
    lowR_.setShelf(120.0f, lowDb_, sampleRate, false);
    midL_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    midR_.setPeaking(midFreq_, midQ_, midDb_, sampleRate);
    highL_.setShelf(6000.0f, highDb_, sampleRate, true);
    highR_.setShelf(6000.0f, highDb_, sampleRate, true);
    sr_ = sampleRate;
    dirty_ = false;
}

void ParametricEQ::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    if (dirty_ || sr_ != sampleRate) {
        recompute(sampleRate);
    }
    for (int i = 0; i < frames; ++i) {
        stereo[2 * i] = highL_.process(midL_.process(lowL_.process(stereo[2 * i])));
        stereo[2 * i + 1] = highR_.process(midR_.process(lowR_.process(stereo[2 * i + 1])));
    }
}

// ---- Bitcrusher -------------------------------------------------------------

void Bitcrusher::reset() {
    holdL_ = 0.0f;
    holdR_ = 0.0f;
    counter_ = 0;
}

void Bitcrusher::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float bits = std::clamp(bits_, 1.0f, 16.0f);
    const int step = std::max(1, static_cast<int>(downsample_));
    const float levels = std::pow(2.0f, bits);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    auto crush = [levels](float x) {
        return std::round(x * levels) / levels; // quantize to `bits` bits
    };
    for (int i = 0; i < frames; ++i) {
        if (counter_ <= 0) {
            holdL_ = crush(stereo[2 * i]);
            holdR_ = crush(stereo[2 * i + 1]);
            counter_ = step;
        }
        --counter_;
        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + holdL_ * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + holdR_ * mix;
    }
}

// ---- Phaser -----------------------------------------------------------------

void Phaser::reset() {
    fbL_ = 0.0f;
    fbR_ = 0.0f;
    phase_ = 0.0;
    for (Allpass1& a : apL_) {
        a.z = 0.0f;
    }
    for (Allpass1& a : apR_) {
        a.z = 0.0f;
    }
}

void Phaser::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr double kTwoPi = 6.283185307179586;
    const double phaseInc = static_cast<double>(rateHz_) / static_cast<double>(sampleRate);
    const float depth = std::clamp(depth_, 0.0f, 1.0f);
    const float fb = std::clamp(feedback_, 0.0f, 0.9f);
    const float mix = std::clamp(mix_, 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const float lfo = static_cast<float>(std::sin(phase_ * kTwoPi));
        // Sweep the all-pass coefficient across the audio band.
        const float a = 0.5f + 0.45f * depth * lfo;

        float xL = stereo[2 * i] + fbL_ * fb;
        for (Allpass1& stage : apL_) {
            xL = stage.process(xL, a);
        }
        fbL_ = xL;

        float xR = stereo[2 * i + 1] + fbR_ * fb;
        for (Allpass1& stage : apR_) {
            xR = stage.process(xR, a);
        }
        fbR_ = xR;

        stereo[2 * i] = stereo[2 * i] * (1.0f - mix) + xL * mix;
        stereo[2 * i + 1] = stereo[2 * i + 1] * (1.0f - mix) + xR * mix;

        phase_ += phaseInc;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }
}

// ---- LowPass ----------------------------------------------------------------

void LowPass::reset() {
    yL_ = 0.0f;
    yR_ = 0.0f;
}

void LowPass::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    constexpr float kTwoPi = 6.283185307179586f;
    const float a =
        1.0f - std::exp(-kTwoPi * std::clamp(cutoff_, 20.0f, static_cast<float>(sampleRate) * 0.49f) /
                        static_cast<float>(sampleRate));
    for (int i = 0; i < frames; ++i) {
        yL_ += a * (stereo[2 * i] - yL_);
        yR_ += a * (stereo[2 * i + 1] - yR_);
        stereo[2 * i] = yL_;
        stereo[2 * i + 1] = yR_;
    }
}

// ---- Compressor -------------------------------------------------------------

void Compressor::reset() {
    env_ = 0.0f;
}

void Compressor::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    const float atkCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float relCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));
    const float makeup = dbToLin(makeupDb_);
    const float ratio = std::max(ratio_, 1.0f);

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float peak = std::max(std::fabs(l), std::fabs(r));

        // Peak-following envelope (fast attack, slow release).
        const float coef = peak > env_ ? atkCoef : relCoef;
        env_ = coef * env_ + (1.0f - coef) * peak;

        // Static gain computer in dB, above the threshold only.
        float gain = 1.0f;
        const float envDb = linToDb(env_);
        if (envDb > thresholdDb_) {
            const float targetDb = thresholdDb_ + (envDb - thresholdDb_) / ratio;
            gain = dbToLin(targetDb - envDb);
        }
        gain *= makeup;
        stereo[2 * i] = l * gain;
        stereo[2 * i + 1] = r * gain;
    }
}

// ---- Gate -------------------------------------------------------------------

void Gate::reset() {
    env_ = 0.0f;
    gain_ = 1.0f;
}

void Gate::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    const float sr = static_cast<float>(sampleRate);
    // Fast detector attack, moderate detector release, plus gain-smoothing coefs for the gate.
    const float detCoef = std::exp(-1.0f / (0.001f * sr));                                // ~1 ms
    const float openCoef = std::exp(-1.0f / (std::max(attackMs_, 0.01f) * 0.001f * sr));
    const float closeCoef = std::exp(-1.0f / (std::max(releaseMs_, 0.01f) * 0.001f * sr));
    const float ratio = std::max(ratio_, 1.0f);
    const float floorLin = dbToLin(rangeDb_);

    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float peak = std::max(std::fabs(l), std::fabs(r));

        // Peak-following detector (fast).
        env_ = detCoef * env_ + (1.0f - detCoef) * peak;
        const float envDb = linToDb(env_);

        // Below threshold → downward expansion toward the floor; above → unity.
        float target = 1.0f;
        if (envDb < thresholdDb_) {
            const float reductionDb = (thresholdDb_ - envDb) * (ratio - 1.0f);
            target = dbToLin(-reductionDb);
            if (target < floorLin) {
                target = floorLin;
            }
        }

        // Smooth the gate gain (open faster, close slower).
        const float coef = target > gain_ ? openCoef : closeCoef;
        gain_ = coef * gain_ + (1.0f - coef) * target;

        stereo[2 * i] = l * gain_;
        stereo[2 * i + 1] = r * gain_;
    }
}

// ---- TapeSaturation ---------------------------------------------------------

void TapeSaturation::reset() {
    lpL_ = 0.0f;
    lpR_ = 0.0f;
}

void TapeSaturation::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    // Compensate for the tanh gain so the wet level stays close to the input.
    const float norm = 1.0f / std::tanh(drive_);
    // One-pole high-cut coefficient: warmth 0 → open (~18 kHz), warmth 1 → ~3 kHz.
    const float cutHz = 18000.0f - warmth_ * 15000.0f;
    const float x = std::exp(-2.0f * 3.14159265f * cutHz / static_cast<float>(sampleRate));
    const float bias = 0.05f * warmth_;
    const float biasOut = std::tanh(bias) * norm; // steady-state DC from the asymmetry, removed below
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        // Asymmetric drive (small bias → even harmonics), tanh soft-knee, gain-compensated, DC-free.
        float wl = std::tanh(drive_ * l + bias) * norm - biasOut;
        float wr = std::tanh(drive_ * r + bias) * norm - biasOut;
        // Gentle high-frequency roll-off (tape top-end loss).
        lpL_ = (1.0f - x) * wl + x * lpL_;
        lpR_ = (1.0f - x) * wr + x * lpR_;
        wl = lpL_;
        wr = lpR_;
        stereo[2 * i] = l + (wl - l) * mix_;
        stereo[2 * i + 1] = r + (wr - r) * mix_;
    }
}

// ---- StereoWidener ----------------------------------------------------------

void StereoWidener::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    for (int i = 0; i < frames; ++i) {
        const float l = stereo[2 * i];
        const float r = stereo[2 * i + 1];
        const float mid = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width_;
        stereo[2 * i] = mid + side;
        stereo[2 * i + 1] = mid - side;
    }
}

// ---- Reverb -----------------------------------------------------------------

void Reverb::Comb::setSize(int n) {
    buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f);
    idx = 0;
    store = 0.0f;
}

float Reverb::Comb::process(float in, float feedback, float damp) {
    const float y = buf[static_cast<size_t>(idx)];
    store = y * (1.0f - damp) + store * damp;
    buf[static_cast<size_t>(idx)] = in + store * feedback;
    if (++idx >= static_cast<int>(buf.size())) {
        idx = 0;
    }
    return y;
}

void Reverb::Allpass::setSize(int n) {
    buf.assign(static_cast<size_t>(std::max(1, n)), 0.0f);
    idx = 0;
}

float Reverb::Allpass::process(float in, float feedback) {
    const float b = buf[static_cast<size_t>(idx)];
    const float y = -in + b;
    buf[static_cast<size_t>(idx)] = in + b * feedback;
    if (++idx >= static_cast<int>(buf.size())) {
        idx = 0;
    }
    return y;
}

void Reverb::ensureSized(int sampleRate) {
    if (sizedFor_ == sampleRate) {
        return;
    }
    sizedFor_ = sampleRate;
    // Freeverb tunings (samples @ 44.1 kHz), scaled to the actual rate. The right channel is offset
    // by a "stereo spread" so the two sides decorrelate.
    const int combTune[kCombs] = {1116, 1188, 1277, 1356};
    const int apTune[kAllpass] = {556, 441};
    const int spread = 23;
    const double scale = static_cast<double>(sampleRate) / 44100.0;
    for (int i = 0; i < kCombs; ++i) {
        combsL_[static_cast<size_t>(i)].setSize(static_cast<int>(combTune[i] * scale));
        combsR_[static_cast<size_t>(i)].setSize(static_cast<int>((combTune[i] + spread) * scale));
    }
    for (int i = 0; i < kAllpass; ++i) {
        apsL_[static_cast<size_t>(i)].setSize(static_cast<int>(apTune[i] * scale));
        apsR_[static_cast<size_t>(i)].setSize(static_cast<int>((apTune[i] + spread) * scale));
    }
}

void Reverb::reset() {
    for (Comb& c : combsL_) {
        std::fill(c.buf.begin(), c.buf.end(), 0.0f);
        c.store = 0.0f;
    }
    for (Comb& c : combsR_) {
        std::fill(c.buf.begin(), c.buf.end(), 0.0f);
        c.store = 0.0f;
    }
    for (Allpass& a : apsL_) {
        std::fill(a.buf.begin(), a.buf.end(), 0.0f);
    }
    for (Allpass& a : apsR_) {
        std::fill(a.buf.begin(), a.buf.end(), 0.0f);
    }
}

void Reverb::process(float* stereo, int frames, int sampleRate) {
    if (!enabled_ || frames <= 0 || sampleRate <= 0) {
        return;
    }
    ensureSized(sampleRate);
    const float feedback = 0.7f + 0.28f * std::clamp(roomSize_, 0.0f, 1.0f);
    const float damp = std::clamp(damping_, 0.0f, 1.0f) * 0.4f;
    const float mix = std::clamp(mix_, 0.0f, 1.0f);
    constexpr float kInputGain = 0.15f;

    for (int i = 0; i < frames; ++i) {
        const float dryL = stereo[2 * i];
        const float dryR = stereo[2 * i + 1];
        const float in = (dryL + dryR) * kInputGain;

        float wetL = 0.0f;
        float wetR = 0.0f;
        for (int c = 0; c < kCombs; ++c) {
            wetL += combsL_[static_cast<size_t>(c)].process(in, feedback, damp);
            wetR += combsR_[static_cast<size_t>(c)].process(in, feedback, damp);
        }
        for (int a = 0; a < kAllpass; ++a) {
            wetL = apsL_[static_cast<size_t>(a)].process(wetL, 0.5f);
            wetR = apsR_[static_cast<size_t>(a)].process(wetR, 0.5f);
        }

        stereo[2 * i] = dryL * (1.0f - mix) + wetL * mix;
        stereo[2 * i + 1] = dryR * (1.0f - mix) + wetR * mix;
    }
}

} // namespace maz::audio
