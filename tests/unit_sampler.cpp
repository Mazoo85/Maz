// Unit test for the A4 multisampler — the WAV reader and the Sampler's pitch-shifted playback. It
// synthesizes a 220 Hz sine, writes it to a WAV, loads it back, and checks that playing it at the
// base note reproduces 220 Hz while an octave up plays 440 Hz. Pure DSP, no audio device.

#include "maz/audio/Pitch.hpp"
#include "maz/audio/Sampler.hpp"
#include "maz/audio/WavReader.hpp"
#include "maz/audio/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

double estimateHz(const std::vector<float>& mono, int sampleRate) {
    if (mono.size() < 2) {
        return 0.0;
    }
    int crossings = 0;
    float prev = mono[0];
    for (size_t i = 1; i < mono.size(); ++i) {
        if (prev <= 0.0f && mono[i] > 0.0f) {
            ++crossings;
        }
        prev = mono[i];
    }
    return static_cast<double>(crossings) * static_cast<double>(sampleRate) /
           static_cast<double>(mono.size());
}

std::vector<float> renderMono(audio::Sampler& s, int frames, int sampleRate) {
    std::vector<float> buf(static_cast<size_t>(frames), 0.0f);
    s.render(buf.data(), frames, sampleRate);
    return buf;
}

} // namespace

int main() {
    const int sr = 48000;
    constexpr double kTwoPi = 6.283185307179586;

    // Synthesize 0.5 s of a 220 Hz sine and write it as a mono WAV.
    const int srcFrames = sr / 2;
    std::vector<float> sine(static_cast<size_t>(srcFrames));
    for (int i = 0; i < srcFrames; ++i) {
        sine[static_cast<size_t>(i)] = 0.8f * static_cast<float>(std::sin(kTwoPi * 220.0 * i / sr));
    }
    const std::string path = "unit_sampler_tone.wav";
    std::string err;
    check(audio::writeWav16(path, sine.data(), srcFrames, 1, sr, &err), "write sample WAV");

    // Read it back through the WAV reader.
    audio::WavData wav;
    check(audio::readWav16(path, wav, &err), "read sample WAV");
    check(wav.channels == 1 && wav.sampleRate == sr, "WAV reports mono @48 kHz");
    check(wav.frames() == srcFrames, "WAV frame count matches");

    // Load into the sampler with base note A3 (MIDI 57 = 220 Hz).
    audio::Sampler sampler;
    check(sampler.load(path, &err), "sampler loads the WAV");
    check(sampler.loaded(), "sampler reports loaded");
    sampler.setBasePitch(57); // 220 Hz

    // Play the base note → back at 220 Hz.
    sampler.noteOn(57, 1.0f);
    check(sampler.active(), "sampler active after noteOn");
    const std::vector<float> atBase = renderMono(sampler, sr / 5, sr); // 0.2 s
    check(std::fabs(estimateHz(atBase, sr) - 220.0) < 6.0, "base note plays at 220 Hz");

    // Play an octave up (MIDI 69 = 440 Hz) → resampled to 440 Hz.
    audio::Sampler sampler2;
    sampler2.load(path, &err);
    sampler2.setBasePitch(57);
    sampler2.noteOn(69, 1.0f);
    const std::vector<float> octaveUp = renderMono(sampler2, sr / 5, sr);
    check(std::fabs(estimateHz(octaveUp, sr) - 440.0) < 10.0, "octave-up note plays at 440 Hz");

    // Key tracking off: every key plays the sample at its natural pitch, so the octave-up note (69)
    // still comes out at 220 Hz (one-shot / drum-sampler mode), not resampled to 440 Hz.
    audio::Sampler noKey;
    noKey.load(path, &err);
    noKey.setBasePitch(57);
    noKey.setKeyTrack(false);
    noKey.noteOn(69, 1.0f);
    const std::vector<float> fixed = renderMono(noKey, sr / 5, sr);
    check(std::fabs(estimateHz(fixed, sr) - 220.0) < 6.0,
          "key tracking off plays every note at the sample's natural pitch (220 Hz)");
    check(audio::Sampler().keyTrack(), "sampler key tracking defaults to on (melodic)");

    // Directly injected mono sample works too.
    audio::Sampler sampler3;
    sampler3.setSampleMono(sine, sr);
    sampler3.setBasePitch(57);
    sampler3.noteOn(57, 1.0f);
    check(estimateHz(renderMono(sampler3, sr / 5, sr), sr) > 100.0, "injected sample plays");

    // Drive: tanh saturation adds (odd) harmonics to an otherwise-clean sine sample.
    {
        auto goertzel = [&](const std::vector<float>& b, double f) {
            const double w = kTwoPi * f / sr;
            const double c = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : b) {
                const double s0 = static_cast<double>(v) + c * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - c * s1 * s2;
        };
        auto thirdHarmonic = [&](float drive) {
            audio::Sampler s;
            s.setSampleMono(sine, sr);
            s.setBasePitch(57); // 220 Hz → 3rd harmonic at 660 Hz
            s.setDrive(drive);
            s.noteOn(57, 1.0f);
            return goertzel(renderMono(s, sr / 4, sr), 660.0);
        };
        check(thirdHarmonic(1.0f) > thirdHarmonic(0.0f) * 20.0 + 1.0,
              "sampler drive adds harmonics to a clean sine");
        check(audio::Sampler().drive() == 0.0f, "sampler drive defaults to 0 (clean)");
    }

    // Pitch envelope: a note starts pitched away and slides to its true pitch.
    {
        const int win = sr / 20; // 50 ms measurement window

        audio::Sampler pe;
        pe.setSampleMono(sine, sr);
        pe.setBasePitch(57); // 220 Hz at the base note
        check(pe.pitchEnvDepth() == 0.0f, "sampler pitch-env defaults to off");
        pe.noteOn(57, 1.0f);
        const double flatHz = estimateHz(renderMono(pe, win, sr), sr);

        // +12 semitones sliding down over 150 ms → the first 50 ms sits well above the base pitch.
        audio::Sampler pe2;
        pe2.setSampleMono(sine, sr);
        pe2.setBasePitch(57);
        pe2.setPitchEnv(12.0f, 0.15f);
        check(std::fabs(pe2.pitchEnvDepth() - 12.0f) < 1e-6f &&
                  std::fabs(pe2.pitchEnvTime() - 0.15f) < 1e-6f,
              "setPitchEnv stores depth + time");
        pe2.noteOn(57, 1.0f);
        const double sweepHz = estimateHz(renderMono(pe2, win, sr), sr);
        check(sweepHz > flatHz * 1.3, "a positive pitch env starts the note above its true pitch");

        // Ranges clamp.
        audio::Sampler pc;
        pc.setPitchEnv(100.0f, 10.0f);
        check(std::fabs(pc.pitchEnvDepth() - 36.0f) < 1e-6f, "pitch-env depth clamps to 36 st");
        check(std::fabs(pc.pitchEnvTime() - 2.0f) < 1e-6f, "pitch-env time clamps to 2 s");
    }

    // Velocity → filter cutoff: harder hits open the low-pass (brighter), amplitude-independent.
    {
        // A broadband (white-noise) sample so the filter's brightness change is measurable.
        std::vector<float> noise(static_cast<size_t>(sr));
        uint32_t rng = 0x1234567u;
        for (int i = 0; i < sr; ++i) {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            noise[static_cast<size_t>(i)] = static_cast<float>(rng) / 2147483648.0f - 1.0f;
        }
        // Brightness = HF (first-difference) energy / total energy — invariant to loudness.
        auto brightness = [&](float velo, float vel) {
            audio::Sampler s;
            s.setSampleMono(noise, sr);
            s.setBasePitch(60);
            s.setFilter(700.0f, 0.7f); // a low base cutoff so velocity has room to open it
            s.setFilterVelo(velo);
            s.noteOn(60, vel);
            const std::vector<float> out = renderMono(s, sr / 10, sr);
            double hf = 0.0, en = 0.0;
            for (size_t i = 0; i < out.size(); ++i) {
                if (i > 0) {
                    const double d = static_cast<double>(out[i]) - static_cast<double>(out[i - 1]);
                    hf += d * d;
                }
                en += static_cast<double>(out[i]) * static_cast<double>(out[i]);
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        audio::Sampler defs;
        check(defs.filterVelo() == 0.0f, "sampler velocity→cutoff defaults to off");
        // With the mod on, a hard hit is brighter than a soft one.
        check(brightness(9000.0f, 1.0f) > brightness(9000.0f, 0.3f) * 1.2,
              "velocity opens the sampler filter (harder = brighter)");
        // With the mod off, brightness is the same regardless of velocity (loudness cancels out).
        const double offSoft = brightness(0.0f, 0.3f);
        const double offHard = brightness(0.0f, 1.0f);
        check(std::fabs(offHard - offSoft) < offSoft * 0.15,
              "with velocity→cutoff off, brightness is velocity-independent");
    }

    // Monophonic mode: overlapping notes collapse to a single voice (last-note priority).
    {
        std::vector<float> tone(static_cast<size_t>(sr) / 2);
        for (int i = 0; i < sr / 2; ++i) {
            tone[static_cast<size_t>(i)] =
                0.5f * static_cast<float>(std::sin(kTwoPi * 220.0 * i / sr));
        }
        // Polyphonic (default): two overlapping notes sound two voices.
        audio::Sampler poly;
        poly.setSampleMono(tone, sr);
        poly.setBasePitch(57);
        check(!poly.mono(), "sampler defaults to polyphonic");
        poly.noteOn(60, 1.0f);
        poly.noteOn(64, 1.0f);
        check(poly.activeVoices() == 2, "polyphonic sampler stacks overlapping notes");

        // Mono: the second note steals the first, so only one voice sounds.
        audio::Sampler mono;
        mono.setSampleMono(tone, sr);
        mono.setBasePitch(57);
        mono.setMono(true);
        mono.noteOn(60, 1.0f);
        mono.noteOn(64, 1.0f);
        check(mono.activeVoices() == 1, "mono sampler plays a single voice (last-note priority)");
        // The surviving voice is the most recent note (64) — it still sounds.
        const std::vector<float> out = renderMono(mono, sr / 10, sr);
        double e = 0.0;
        for (float s : out) {
            e += static_cast<double>(s) * static_cast<double>(s);
        }
        check(mono.active() && e > 0.0, "the stolen-to mono voice keeps sounding");
    }

    // Amp ADSR: after the attack peaks, the level decays to the sustain and holds there while held.
    {
        std::vector<float> tone(static_cast<size_t>(sr));
        for (int i = 0; i < sr; ++i) {
            tone[static_cast<size_t>(i)] = 0.6f * static_cast<float>(std::sin(kTwoPi * 220.0 * i / sr));
        }
        auto sustainRms = [&](float sustain) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setBasePitch(60);
            s.setAmpEnv(0.0005f, 0.1f); // ~24-frame attack
            s.setAmpDecay(0.01f);       // ~480-frame decay
            s.setAmpSustain(sustain);
            s.noteOn(60, 1.0f); // read at natural speed, note held (no noteOff)
            const std::vector<float> out = renderMono(s, sr / 2, sr); // 0.5 s, past attack+decay
            double en = 0.0;
            int n = 0;
            for (int i = sr / 3; i < sr / 2; ++i) { // steady-state sustain window
                en += static_cast<double>(out[static_cast<size_t>(i)]) * out[static_cast<size_t>(i)];
                ++n;
            }
            return std::sqrt(en / n);
        };
        const double full = sustainRms(1.0f);
        const double half = sustainRms(0.5f);
        check(full > 0.0, "a held sample sustains at the full level (sustain 1)");
        check(std::fabs(half - full * 0.5) < full * 0.1,
              "amp sustain 0.5 holds at half the full-sustain level");
        audio::Sampler def;
        check(std::fabs(def.ampSustain() - 1.0f) < 1e-6f, "amp sustain defaults to 1 (transparent)");
    }

    // Velocity → volume sensitivity: full (default) makes a soft hit quieter; zero ignores velocity.
    {
        std::vector<float> tone(static_cast<size_t>(sr) / 2);
        for (int i = 0; i < sr / 2; ++i) {
            tone[static_cast<size_t>(i)] = 0.6f * static_cast<float>(std::sin(kTwoPi * 220.0 * i / sr));
        }
        auto hitRms = [&](float velSens, float velocity) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setBasePitch(60);
            s.setVelSensitivity(velSens);
            s.noteOn(60, velocity);
            const std::vector<float> out = renderMono(s, sr / 4, sr);
            double e = 0.0;
            for (float v : out) {
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return std::sqrt(e / static_cast<double>(out.size()));
        };
        const double hard = hitRms(1.0f, 1.0f);
        const double soft = hitRms(1.0f, 0.3f);
        check(soft < hard * 0.5, "full velocity sensitivity makes a soft hit quieter");
        const double softFlat = hitRms(0.0f, 0.3f);
        const double hardFlat = hitRms(0.0f, 1.0f);
        check(std::fabs(softFlat - hardFlat) < hardFlat * 0.02,
              "zero velocity sensitivity ignores velocity for volume");
        audio::Sampler dv;
        check(std::fabs(dv.velSensitivity() - 1.0f) < 1e-6f, "velocity sensitivity defaults to 1 (full)");
    }

    // Reverse playback: a ramp sample read backwards starts near the end value and descends.
    {
        std::vector<float> ramp(1000);
        for (int i = 0; i < 1000; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / 1000.0f; // 0 → ~1
        }
        audio::Sampler fwd;
        fwd.setSampleMono(ramp, sr);
        fwd.setBasePitch(60);
        fwd.noteOn(60, 1.0f); // play at base pitch → 1 sample/frame
        const std::vector<float> f = renderMono(fwd, 200, sr);

        audio::Sampler rev;
        rev.setSampleMono(ramp, sr);
        rev.setBasePitch(60);
        rev.setReverse(true);
        rev.noteOn(60, 1.0f);
        const std::vector<float> r = renderMono(rev, 200, sr);
        // Forward starts low (near 0); reverse starts high (near 1, the end of the ramp).
        check(r[100] > f[100] + 0.5f, "reverse playback reads the sample backwards (starts high)");
    }

    // Looping: a one-shot that would end still sounds well past its length when looped.
    {
        std::vector<float> sh = sine; // ~short sample
        audio::Sampler once;
        once.setSampleMono(sh, sr);
        once.noteOn(57, 1.0f);
        const int longRender = static_cast<int>(sh.size()) * 3;
        const std::vector<float> o = renderMono(once, longRender, sr);
        check(!once.active(), "a non-looped one-shot ends after the sample");

        audio::Sampler looped;
        looped.setSampleMono(sh, sr);
        looped.setLoop(true);
        looped.noteOn(57, 1.0f);
        const std::vector<float> lp = renderMono(looped, longRender, sr);
        check(looped.active(), "a looped sample keeps sounding past its length");
        double secondPassEnergy = 0.0;
        for (size_t i = sh.size() + 100; i < lp.size(); ++i) {
            secondPassEnergy += static_cast<double>(lp[i]) * static_cast<double>(lp[i]);
        }
        check(secondPassEnergy > 0.0, "looped sample produces sound in the second pass");
    }

    // Ping-pong loop: playback bounces off the ends instead of wrapping, so it traverses the sample
    // both ways with no wrap discontinuity (unlike a plain loop).
    {
        std::vector<float> ramp(100);
        for (int i = 0; i < 100; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / 100.0f; // 0 → 0.99
        }
        auto analyze = [&](bool pingpong, float& maxJump, float& lo, float& hi) {
            audio::Sampler s;
            s.setSampleMono(ramp, sr);
            s.setBasePitch(60);
            s.setGain(1.0f);
            s.setLoop(true);
            s.setPingPong(pingpong);
            s.setAmpEnv(0.0001f, 0.1f); // env settles in ~5 frames
            s.noteOn(60, 1.0f);         // play at base pitch → 1 sample/frame
            const std::vector<float> out = renderMono(s, 300, sr);
            maxJump = 0.0f;
            lo = 1e9f;
            hi = -1e9f;
            for (size_t i = 11; i < out.size(); ++i) { // skip the attack ramp
                const float j = std::fabs(out[i] - out[i - 1]);
                if (j > maxJump) maxJump = j;
                if (out[i] < lo) lo = out[i];
                if (out[i] > hi) hi = out[i];
            }
        };
        float ppJump = 0.0f, ppLo = 0.0f, ppHi = 0.0f;
        analyze(true, ppJump, ppLo, ppHi);
        float plJump = 0.0f, plLo = 0.0f, plHi = 0.0f;
        analyze(false, plJump, plLo, plHi);
        check(ppHi > 0.9f && ppLo < 0.3f, "ping-pong traverses the full sample both ways");
        check(ppJump < 0.05f, "ping-pong loop bounces smoothly (no wrap discontinuity)");
        check(plJump > 0.5f, "a plain loop wraps with a discontinuity (contrast)");
        audio::Sampler dp;
        check(!dp.pingPong(), "ping-pong defaults off");
    }

    // Normalize: bring a quiet sample's peak up to full scale, preserving its shape.
    {
        std::vector<float> quiet(200);
        for (int i = 0; i < 200; ++i) {
            quiet[static_cast<size_t>(i)] =
                static_cast<float>(0.2 * std::sin(2.0 * 3.14159265358979 * i / 200.0)); // peak 0.2
        }
        audio::Sampler s;
        s.setSampleMono(quiet, sr);
        check(std::fabs(s.samplePeak() - 0.2f) < 0.01f, "sample peak is reported before normalize");
        s.normalize();
        check(std::fabs(s.samplePeak() - 1.0f) < 1e-4f, "normalize brings the peak to full scale");

        // A silent sample is left alone (no divide-by-zero).
        audio::Sampler z;
        std::vector<float> silent(50, 0.0f);
        z.setSampleMono(silent, sr);
        z.normalize();
        check(z.samplePeak() == 0.0f, "normalizing a silent sample is a no-op");
    }

    // Fade edges: linear fade-in/out on a flat sample ramps the ends to zero, middle untouched.
    {
        std::vector<float> flat(1000, 1.0f);
        audio::Sampler s;
        s.setSampleMono(flat, sr);
        check(s.sampleLength() == 1000, "sample length is reported");
        s.fadeEdges(5.0f); // 5 ms → 240 frames at 48 kHz
        check(s.sampleValue(0) < 0.05f, "fade-in starts near zero");
        check(std::fabs(s.sampleValue(120) - 0.5f) < 0.1f, "fade-in ramps to ~half at its midpoint");
        check(std::fabs(s.sampleValue(500) - 1.0f) < 1e-4f, "the sample middle is left untouched");
        check(s.sampleValue(999) < 0.05f, "fade-out ends near zero");
    }

    // Fine tune: +1200 cents (an octave) roughly doubles the played pitch.
    {
        std::vector<float> tone(static_cast<size_t>(sr));
        for (int i = 0; i < sr; ++i) {
            tone[static_cast<size_t>(i)] =
                static_cast<float>(std::sin(2.0 * 3.14159265358979 * 300.0 * i / sr));
        }
        auto crossings = [&](float cents) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setBasePitch(60);
            s.setDetuneCents(cents);
            s.noteOn(60, 1.0f); // play at base pitch → rate driven only by the detune
            const std::vector<float> out = renderMono(s, sr / 2, sr); // mono buffer
            int c = 0;
            for (int i = 1; i < sr / 2; ++i) {
                if (out[static_cast<size_t>(i - 1)] <= 0.0f && out[static_cast<size_t>(i)] > 0.0f) {
                    ++c;
                }
            }
            return c;
        };
        const int baseCross = crossings(0.0f);
        const int octCross = crossings(1200.0f);
        check(baseCross > 0, "sampler plays the sample at its base pitch");
        check(octCross > baseCross * 1.7, "a +1200-cent fine tune plays the sample about an octave up");
        audio::Sampler d;
        check(d.detuneCents() == 0.0f, "sampler fine tune defaults to 0");
    }

    // Start offset: playback begins partway into the sample (skips the leading part).
    {
        // A ramp 0→1: reading from offset 0.5 starts near value 0.5, not 0.
        std::vector<float> ramp(1000);
        for (int i = 0; i < 1000; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / 1000.0f;
        }
        audio::Sampler s0;
        s0.setSampleMono(ramp, sr);
        s0.setBasePitch(60);
        s0.setGain(1.0f);
        s0.noteOn(60, 1.0f); // 1 sample/frame
        const std::vector<float> from0 = renderMono(s0, 100, sr);

        audio::Sampler sHalf;
        sHalf.setSampleMono(ramp, sr);
        sHalf.setBasePitch(60);
        sHalf.setGain(1.0f);
        sHalf.setStartOffset(0.5f);
        sHalf.noteOn(60, 1.0f);
        const std::vector<float> fromHalf = renderMono(sHalf, 100, sr);
        // Compare at frame 60 (past the ~1 ms attack, so the envelope is ~open).
        check(fromHalf[60] > from0[60] + 0.4f, "start offset begins playback partway into the sample");
        check(fromHalf[60] > 0.5f && fromHalf[60] < 0.62f, "offset 0.5 starts near the sample midpoint");

        audio::Sampler def;
        check(def.startOffset() == 0.0f, "start offset defaults to 0");
    }

    // Loop region: with loop on and a region [0.6, 0.8], the attack head plays once, then playback
    // sustains only within that band — a ramp source stays between ~0.6 and ~0.8 forever, never
    // reaching the 0..1 extremes a whole-sample loop would.
    {
        std::vector<float> ramp(1000);
        for (int i = 0; i < 1000; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / 1000.0f; // 0 → ~1
        }
        auto steadyBand = [&](bool region, float& lo, float& hi) {
            audio::Sampler s;
            s.setSampleMono(ramp, sr);
            s.setBasePitch(60);
            s.setGain(1.0f);
            s.setAmpEnv(0.0001f, 0.1f);
            s.setLoop(true);
            if (region) {
                s.setLoopRegion(0.6f, 0.8f);
            }
            s.noteOn(60, 1.0f); // 1 sample/frame
            const std::vector<float> out = renderMono(s, 3000, sr);
            lo = 1e9f;
            hi = -1e9f;
            for (size_t i = 1000; i < out.size(); ++i) { // steady state, past the head + attack
                if (out[i] < lo) lo = out[i];
                if (out[i] > hi) hi = out[i];
            }
        };
        float rLo = 0.0f, rHi = 0.0f;
        steadyBand(true, rLo, rHi);
        check(rHi < 0.85f && rLo > 0.55f, "a loop region sustains only within [0.6, 0.8]");

        float wLo = 0.0f, wHi = 0.0f;
        steadyBand(false, wLo, wHi);
        check(wHi > 0.95f && wLo < 0.1f, "a whole-sample loop traverses the full 0..1 range (contrast)");

        audio::Sampler def;
        check(def.loopStart() == 0.0f && def.loopEnd() == 1.0f,
              "loop region defaults to the whole sample");
        def.setLoopRegion(0.8f, 0.2f); // end <= start is ignored
        check(def.loopStart() == 0.0f && def.loopEnd() == 1.0f,
              "a degenerate loop region (end <= start) is ignored");
    }

    // Loop crossfade: smooth the seam where playback wraps loopEnd → loopStart.
    {
        // A linear ramp 0 → 1: with a loop region [0.25, 0.75] the seam jumps from the value near
        // loopEnd (~0.75) down to the value at loopStart (~0.25) — a big discontinuity.
        const int n = 4000;
        std::vector<float> ramp(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / static_cast<float>(n - 1);
        }
        audio::Sampler s;
        s.setSampleMono(ramp, sr);
        s.setLoop(true);
        s.setLoopRegion(0.25f, 0.75f);
        const int ls = static_cast<int>(0.25f * static_cast<float>(n));
        const int le = static_cast<int>(0.75f * static_cast<float>(n));
        const float jumpBefore = std::fabs(s.sampleValue(static_cast<size_t>(le - 1)) -
                                           s.sampleValue(static_cast<size_t>(ls)));
        const int applied = s.crossfadeLoop(10.0f); // 10 ms crossfade
        check(applied > 0, "crossfade applies when there is pre-roll before the loop start");
        const float jumpAfter = std::fabs(s.sampleValue(static_cast<size_t>(le - 1)) -
                                          s.sampleValue(static_cast<size_t>(ls)));
        check(jumpAfter < jumpBefore * 0.3f,
              "loop crossfade shrinks the discontinuity at the loop seam");

        // No pre-roll (loopStart at 0) → nothing to blend from, a clean no-op.
        audio::Sampler s2;
        s2.setSampleMono(ramp, sr);
        s2.setLoop(true); // region defaults to the whole sample (loopStart 0)
        check(s2.crossfadeLoop(10.0f) == 0, "crossfade is a no-op with no pre-roll (loopStart at 0)");
    }

    // Playback low-pass filter: darken a bright sample.
    {
        std::vector<float> bright(static_cast<size_t>(sr), 0.0f); // 1 s of a bright 6 kHz tone
        for (int i = 0; i < sr; ++i) {
            bright[static_cast<size_t>(i)] = 0.5f * static_cast<float>(std::sin(kTwoPi * 6000.0 * i / sr));
        }
        auto hf = [&](float cutoff) {
            audio::Sampler s;
            s.setSampleMono(bright, sr);
            s.setBasePitch(60);
            s.setFilter(cutoff, 0.7f);
            s.noteOn(60, 1.0f); // natural speed → the 6 kHz survives unless filtered
            const std::vector<float> out = renderMono(s, sr / 4, sr);
            double h = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
            }
            return h;
        };
        check(hf(500.0f) < hf(20000.0f) * 0.5,
              "sampler low-pass filter attenuates a bright sample's highs");
        audio::Sampler df;
        check(df.filterCutoff() == 20000.0f && df.filterResonance() == 0.7f,
              "sampler filter defaults to open (bypass)");

        // Filter envelope: opens the cutoff at the start (bright) then decays it closed (dark).
        audio::Sampler fe;
        fe.setSampleMono(bright, sr);
        fe.setBasePitch(60);
        fe.setFilter(500.0f, 0.7f);          // dark base cutoff
        fe.setFilterEnvDepth(8000.0f);       // envelope opens it wide at the start
        fe.setFilterEnvelope(0.001f, 0.04f, 0.0f, 0.1f); // fast attack, decay closed over ~40 ms
        fe.noteOn(60, 1.0f);
        const std::vector<float> feo = renderMono(fe, sr / 4, sr);
        auto hfWin = [&](const std::vector<float>& b, int a, int c) {
            double h = 0.0;
            for (int i = a + 1; i < c; ++i) {
                const double d = static_cast<double>(b[static_cast<size_t>(i)] - b[static_cast<size_t>(i - 1)]);
                h += d * d;
            }
            return h;
        };
        const int w = sr / 50;                          // 20 ms windows
        const double early = hfWin(feo, 0, w);          // bright (cutoff open)
        const double late = hfWin(feo, 5 * w, 6 * w);   // dark (cutoff decayed shut)
        check(early > late * 2.0,
              "sampler filter envelope opens the cutoff bright at the start then closes it");
        audio::Sampler de;
        check(de.filterEnvDepth() == 0.0f, "sampler filter-envelope depth defaults to 0 (off)");
    }

    // Beat slicer: N slices mapped across the keyboard from the base note; each note plays its slice
    // once at natural speed and stops at the slice boundary.
    {
        std::vector<float> ramp(1000);
        for (int i = 0; i < 1000; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / 1000.0f; // 0 → ~1
        }
        auto sliceStart = [&](int note) {
            audio::Sampler s;
            s.setSampleMono(ramp, sr);
            s.setBasePitch(60);
            s.setGain(1.0f);
            s.setSlices(4); // slice length 250 frames
            s.noteOn(note, 1.0f); // natural speed → 1 sample/frame
            const std::vector<float> out = renderMono(s, 100, sr);
            return out[60]; // past the ~1 ms attack
        };
        const float s0 = sliceStart(60); // slice 0 → starts near 0.0
        const float s1 = sliceStart(61); // slice 1 → starts near 0.25
        const float s2 = sliceStart(62); // slice 2 → starts near 0.50
        check(s1 > s0 + 0.15f && s2 > s1 + 0.15f, "each higher note plays a later slice");
        check(s1 > 0.2f && s1 < 0.4f, "slice 1 begins a quarter into the sample");

        // A slice is a one-shot that ends at its boundary (slice 0 = 250 frames long).
        audio::Sampler one;
        one.setSampleMono(ramp, sr);
        one.setBasePitch(60);
        one.setSlices(4);
        one.noteOn(60, 1.0f);
        (void)renderMono(one, 300, sr);
        check(!one.active(), "a slice ends at its boundary (one-shot)");

        audio::Sampler def;
        check(def.slices() == 1, "slicing defaults to off (1 slice)");
    }

    // Amp envelope: a longer release makes the note-off tail ring longer before going silent.
    {
        std::vector<float> tone(sr, 0.5f); // 1 s of a constant DC-ish level (steady amplitude)
        auto tailLen = [&](float rel) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setLoop(true); // keep the source sounding so only the env governs the tail
            s.setAmpEnv(0.001f, rel);
            s.noteOn(60, 1.0f);
            (void)renderMono(s, 4800, sr); // let attack finish
            s.noteOff(60);
            // Render and find how many frames until the voice goes inactive.
            int frames = 0;
            while (s.active() && frames < sr) {
                std::vector<float> b(64, 0.0f);
                s.render(b.data(), 64, sr);
                frames += 64;
            }
            return frames;
        };
        const int shortT = tailLen(0.01f);
        const int longT = tailLen(0.2f);
        check(longT > shortT * 3, "a longer sampler release rings out longer");

        audio::Sampler def;
        check(std::fabs(def.release() - 0.012f) < 1e-4f, "sampler release has a sane default");
    }

    // Portamento / glide: a new pitched note slides its read speed from the previous note's pitch
    // to its own, so the early pitch after a jump sits below the target and settles onto it.
    {
        const int f = sr / 2;
        std::vector<float> tone(static_cast<size_t>(f));
        for (int i = 0; i < f; ++i) {
            tone[static_cast<size_t>(i)] =
                0.8f * static_cast<float>(std::sin(kTwoPi * 440.0 * i / sr));
        }
        // Play A3 (220 Hz), settle, then jump to A4 (440 Hz); measure the first 50 ms after the jump.
        auto earlyHzAfterJump = [&](float glide) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setBasePitch(69); // 440 Hz at note 69
            s.setLoop(true);
            s.setMono(true);
            s.setGlide(glide);
            s.noteOn(57, 1.0f);               // an octave down → 220 Hz
            (void)renderMono(s, sr / 10, sr); // settle at 220 Hz
            s.noteOn(69, 1.0f);               // up to 440 Hz (glides when enabled)
            return estimateHz(renderMono(s, sr / 20, sr), sr);
        };
        const double snapHz = earlyHzAfterJump(0.0f);
        const double glideHz = earlyHzAfterJump(0.3f);
        check(snapHz > 380.0, "without glide the new note jumps straight to pitch");
        check(glideHz < 340.0, "with glide the new note starts below its target pitch");
        check(glideHz < snapHz - 40.0, "glide keeps the early pitch lower than an instant jump");

        // A long glide render eventually reaches the target pitch.
        audio::Sampler s2;
        s2.setSampleMono(tone, sr);
        s2.setBasePitch(69);
        s2.setLoop(true);
        s2.setMono(true);
        s2.setGlide(0.2f);
        s2.noteOn(57, 1.0f);
        (void)renderMono(s2, sr / 10, sr);
        s2.noteOn(69, 1.0f);
        (void)renderMono(s2, sr, sr); // 1 s — well past the glide time
        const double settledHz = estimateHz(renderMono(s2, sr / 10, sr), sr);
        check(std::fabs(settledHz - 440.0) < 15.0, "glide settles on the target pitch");

        // Legato glide: an isolated note (nothing held) starts on-pitch even with glide on.
        audio::Sampler leg;
        leg.setSampleMono(tone, sr);
        leg.setBasePitch(69);
        leg.setLoop(true);
        leg.setMono(true);
        leg.setGlide(0.3f);
        leg.setGlideLegato(true);
        leg.noteOn(57, 1.0f); // no prior held note → should not glide
        const double isoHz = estimateHz(renderMono(leg, sr / 20, sr), sr);
        check(std::fabs(isoHz - 220.0) < 20.0, "legato glide leaves an isolated note on-pitch");

        audio::Sampler def;
        check(def.glide() == 0.0f && !def.glideLegato(), "glide defaults to off");
    }

    // Velocity → attack: softer hits swell in more slowly (isolated from velocity→volume).
    {
        std::vector<float> tone(static_cast<size_t>(sr), 0.5f); // 1 s of a steady level
        auto earlyRms = [&](float velocity, float velAtk) {
            audio::Sampler s;
            s.setSampleMono(tone, sr);
            s.setVelSensitivity(0.0f); // level is velocity-independent → only the attack time differs
            s.setAmpEnv(0.05f, 0.1f);  // a 50 ms attack
            s.setVelToAttack(velAtk);
            s.noteOn(60, velocity);
            const std::vector<float> b = renderMono(s, sr / 100, sr); // first 10 ms
            double e = 0.0;
            for (float v : b) {
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return std::sqrt(e / static_cast<double>(b.size()));
        };
        const double softOn = earlyRms(0.2f, 1.0f);
        const double hardOn = earlyRms(1.0f, 1.0f);
        check(hardOn > softOn * 1.5,
              "sampler vel->attack makes a hard hit rise faster (louder early) than a soft one");
        const double softOff = earlyRms(0.2f, 0.0f);
        const double hardOff = earlyRms(1.0f, 0.0f);
        check(std::fabs(hardOff - softOff) < hardOff * 0.05 + 1e-6,
              "with sampler vel->attack off the attack ramp is velocity-independent");
        check(audio::Sampler().velToAttack() == 0.0f, "sampler vel->attack defaults to off");
    }

    // Velocity → start: softer hits begin further into the sample (skipping the transient).
    {
        std::vector<float> ramp(static_cast<size_t>(sr)); // a 0→1 linear ramp: value == position
        for (int i = 0; i < sr; ++i) {
            ramp[static_cast<size_t>(i)] = static_cast<float>(i) / static_cast<float>(sr - 1);
        }
        // Read at natural speed (key-track off) with a near-instant attack and no velocity→volume, so
        // the early output value reflects the read *position* (i.e. how far in the note started).
        auto earlyValue = [&](float velocity, float velToStart) {
            audio::Sampler s;
            s.setSampleMono(ramp, sr);
            s.setKeyTrack(false);
            s.setVelSensitivity(0.0f);
            s.setAmpEnv(0.0002f, 0.05f);
            s.setVelToStart(velToStart);
            s.noteOn(60, velocity);
            const std::vector<float> b = renderMono(s, 64, sr);
            return b[32]; // past the ~10-sample attack, still near the start
        };
        const double hard = static_cast<double>(earlyValue(1.0f, 0.6f)); // full transient → starts ~0
        const double soft = static_cast<double>(earlyValue(0.1f, 0.6f)); // starts deep into the ramp
        check(soft > hard + 0.3, "sampler vel->start makes a soft hit begin further into the sample");
        // With the amount at 0 the start is velocity-independent (both begin at the base offset).
        const double hardOff = static_cast<double>(earlyValue(1.0f, 0.0f));
        const double softOff = static_cast<double>(earlyValue(0.1f, 0.0f));
        check(std::fabs(hardOff - softOff) < 0.02, "with sampler vel->start off the start is fixed");
        check(audio::Sampler().velToStart() == 0.0f, "sampler vel->start defaults to off");
    }

    // Filter keyboard tracking: a higher note opens the playback filter, so it stays brighter.
    {
        // Broadband (noise) sample so the low-pass cutoff clearly gates the high frequencies.
        std::vector<float> noise(static_cast<size_t>(sr));
        uint32_t rng = 0x1234567u;
        for (int i = 0; i < sr; ++i) {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            noise[static_cast<size_t>(i)] = static_cast<float>(rng) / 2147483648.0f - 1.0f;
        }
        auto hfAtNote = [&](int midi, float kt) {
            audio::Sampler s;
            s.setSampleMono(noise, sr);
            s.setKeyTrack(false); // fixed playback pitch → isolate the filter's cutoff from resampling
            s.setBasePitch(60);
            s.setFilter(1000.0f, 0.7f); // a 1 kHz low-pass
            s.setFilterKeyTrack(kt);
            s.noteOn(midi, 1.0f);
            const std::vector<float> b = renderMono(s, sr / 10, sr);
            double hf = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                hf += d * d;
            }
            return hf;
        };
        // With full tracking, note 72 (an octave above base) doubles the cutoff → more HF than note 60.
        check(hfAtNote(72, 1.0f) > hfAtNote(60, 1.0f) * 1.3,
              "sampler filter key-tracking brightens higher notes");
        // With tracking off, the cutoff is fixed, so both notes have ~the same brightness.
        const double lo = hfAtNote(60, 0.0f);
        const double hi = hfAtNote(72, 0.0f);
        check(std::fabs(hi - lo) < lo * 0.15 + 1e-9, "with filter key-track off the cutoff is fixed");
        check(audio::Sampler().filterKeyTrack() == 0.0f, "sampler filter key-track defaults to off");
    }

    // --- TPDF export dither ---------------------------------------------------
    // A constant signal sitting a fraction of an LSB above a quantization step. Without dither every
    // sample rounds to the same integer (a dead-constant, distorted result); with dither the rounding
    // is randomized so the output varies sample-to-sample yet stays unbiased around the true value —
    // the textbook behaviour that trades quantization distortion for a benign noise floor.
    {
        const int n = 4096;
        const float lsb = 1.0f / 32767.0f;
        std::vector<float> flat(static_cast<size_t>(n), 0.3f * lsb); // 0.3 LSB → rounds to 0 undithered

        const std::string dpath = "unit_sampler_dither.wav";
        // No dither: identical write is bit-exact on repeat, and every decoded sample is the same.
        check(audio::writeWav16(dpath, flat.data(), n, 1, sr, &err, false), "write undithered WAV");
        audio::WavData nd;
        check(audio::readWav16(dpath, nd, &err), "read undithered WAV");
        bool allSame = true;
        for (float s : nd.samples) {
            if (s != nd.samples[0]) {
                allSame = false;
                break;
            }
        }
        check(allSame, "undithered constant sub-LSB signal quantizes to a dead-constant output");

        // Dither on: the output is no longer constant (some samples round up), but its mean stays
        // close to the intended 0.3 LSB — the dither is unbiased, not a DC offset.
        check(audio::writeWav16(dpath, flat.data(), n, 1, sr, &err, true), "write dithered WAV");
        audio::WavData dd;
        check(audio::readWav16(dpath, dd, &err), "read dithered WAV");
        bool varies = false;
        double sum = 0.0;
        for (float s : dd.samples) {
            if (s != dd.samples[0]) {
                varies = true;
            }
            sum += static_cast<double>(s);
        }
        check(varies, "dither decorrelates quantization: the output is no longer dead-constant");
        const double mean = sum / static_cast<double>(dd.samples.size());
        check(std::fabs(mean - 0.3 * static_cast<double>(lsb)) < 0.25 * static_cast<double>(lsb),
              "dither is unbiased: mean stays near the true sub-LSB level");

        // Deterministic: the same input dithers to a byte-identical file (fixed-seed PRNG).
        const std::string dpath2 = "unit_sampler_dither2.wav";
        check(audio::writeWav16(dpath2, flat.data(), n, 1, sr, &err, true), "write dithered WAV #2");
        audio::WavData dd2;
        check(audio::readWav16(dpath2, dd2, &err), "read dithered WAV #2");
        bool identical = dd.samples.size() == dd2.samples.size();
        for (size_t i = 0; identical && i < dd.samples.size(); ++i) {
            identical = dd.samples[i] == dd2.samples[i];
        }
        check(identical, "dither is deterministic: same input → byte-identical WAV");
    }

    // --- Export peak-normalization -------------------------------------------
    // Static peak normalization scales the buffer so its loudest sample hits the target, without
    // touching dynamics; it is a no-op on silence, and idempotent once at the target.
    {
        std::vector<float> b = {0.0f, 0.5f, -0.25f, 0.1f}; // peak 0.5
        const float g = audio::peakNormalize(b.data(), static_cast<int>(b.size()), 0.966f);
        check(std::fabs(g - 0.966f / 0.5f) < 1e-4f, "normalize returns target/peak gain");
        float pk = 0.0f;
        for (float s : b) {
            pk = std::max(pk, std::fabs(s));
        }
        check(std::fabs(pk - 0.966f) < 1e-4f, "normalized buffer peaks at the target");
        // Relative shape preserved: the -0.25 sample stays exactly half the +0.5 sample's magnitude.
        check(std::fabs(std::fabs(b[2]) / b[1] - 0.5f) < 1e-4f, "normalize preserves relative levels");

        // Idempotent: normalizing again applies ~unity gain and leaves the peak put.
        const float g2 = audio::peakNormalize(b.data(), static_cast<int>(b.size()), 0.966f);
        check(std::fabs(g2 - 1.0f) < 1e-4f, "re-normalizing an at-target buffer is a no-op");

        // Silence and degenerate targets are no-ops that never divide by zero.
        std::vector<float> silent(16, 0.0f);
        check(audio::peakNormalize(silent.data(), static_cast<int>(silent.size())) == 1.0f,
              "normalize is a no-op on silence");
        std::vector<float> c = {0.4f, -0.2f};
        check(audio::peakNormalize(c.data(), static_cast<int>(c.size()), 0.0f) == 1.0f &&
                  c[0] == 0.4f,
              "normalize with target <= 0 is a no-op");
    }

    // --- Sampler filter mode (multimode SVF: low-pass vs high-pass) ----------
    {
        // A sample carrying a low (120 Hz) and a high (6 kHz) tone. A ~1 kHz filter should keep the
        // low in low-pass mode and the high in high-pass mode.
        const int fn = sr / 2;
        std::vector<float> two(static_cast<size_t>(fn));
        for (int i = 0; i < fn; ++i) {
            two[static_cast<size_t>(i)] =
                0.5f * static_cast<float>(std::sin(kTwoPi * 120.0 * i / sr)) +
                0.5f * static_cast<float>(std::sin(kTwoPi * 6000.0 * i / sr));
        }
        auto goertzel = [](const std::vector<float>& b, double f, int srate) {
            const double w = kTwoPi * f / srate;
            const double cc = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : b) {
                const double s0 = static_cast<double>(v) + cc * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - cc * s1 * s2;
        };
        auto bandsFor = [&](audio::StateVariableFilter::Mode m, double& lo, double& hi) {
            audio::Sampler s;
            s.setSampleMono(two, sr);
            s.setBasePitch(60);
            s.setKeyTrack(false); // play at the sample's natural pitch
            s.setFilter(1000.0f, 0.7f);
            s.setFilterMode(m);
            s.noteOn(60, 1.0f);
            const std::vector<float> out = renderMono(s, fn, sr);
            lo = goertzel(out, 120.0, sr);
            hi = goertzel(out, 6000.0, sr);
        };
        double lpLo = 0, lpHi = 0, hpLo = 0, hpHi = 0;
        bandsFor(audio::StateVariableFilter::Mode::LowPass, lpLo, lpHi);
        bandsFor(audio::StateVariableFilter::Mode::HighPass, hpLo, hpHi);
        check(lpLo > lpHi, "sampler low-pass keeps the low tone over the high");
        check(hpHi > hpLo, "sampler high-pass keeps the high tone over the low");
        check(lpLo > hpLo * 2.0, "high-pass attenuates the low tone relative to low-pass");
        check(hpHi > lpHi * 2.0, "low-pass attenuates the high tone relative to high-pass");
        check(audio::Sampler().filterMode() == audio::StateVariableFilter::Mode::LowPass,
              "sampler filter mode defaults to low-pass");
    }

    // --- Sampler filter LFO (cutoff wobble modulates brightness over time) ---
    {
        const int fn = sr / 2;
        std::vector<float> two(static_cast<size_t>(fn));
        for (int i = 0; i < fn; ++i) {
            two[static_cast<size_t>(i)] =
                0.5f * static_cast<float>(std::sin(kTwoPi * 120.0 * i / sr)) +
                0.5f * static_cast<float>(std::sin(kTwoPi * 6000.0 * i / sr));
        }
        auto goertzel = [](const std::vector<float>& b, double f, int srate) {
            const double w = kTwoPi * f / srate;
            const double cc = 2.0 * std::cos(w);
            double s1 = 0.0, s2 = 0.0;
            for (float v : b) {
                const double s0 = static_cast<double>(v) + cc * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            return s1 * s1 + s2 * s2 - cc * s1 * s2;
        };
        // Depth 0 is a no-op: identical to a sampler that never touched the LFO.
        audio::Sampler base;
        base.setSampleMono(two, sr);
        base.setBasePitch(60);
        base.setKeyTrack(false);
        base.setFilter(3000.0f, 0.7f);
        base.noteOn(60, 1.0f);
        const std::vector<float> plain = renderMono(base, fn, sr);
        audio::Sampler z;
        z.setSampleMono(two, sr);
        z.setBasePitch(60);
        z.setKeyTrack(false);
        z.setFilter(3000.0f, 0.7f);
        z.setFilterLfo(4.0f, 0.0f);
        z.noteOn(60, 1.0f);
        const std::vector<float> zero = renderMono(z, fn, sr);
        check(plain == zero, "sampler filter LFO at depth 0 is a bit-for-bit no-op");

        // A deep, slow LFO sweeps the cutoff past 6 kHz and back, so the 6 kHz band pulses in time.
        audio::Sampler s;
        s.setSampleMono(two, sr);
        s.setBasePitch(60);
        s.setKeyTrack(false);
        s.setFilter(3000.0f, 0.7f);
        s.setFilterLfo(4.0f, 6000.0f);
        s.noteOn(60, 1.0f);
        const std::vector<float> swept = renderMono(s, fn, sr);
        const int w = fn / 4;
        double lo = 1e30, hi = 0.0;
        for (int k = 0; k < 4; ++k) {
            std::vector<float> win(swept.begin() + static_cast<long>(k) * w,
                                   swept.begin() + static_cast<long>(k + 1) * w);
            const double e = goertzel(win, 6000.0, sr);
            lo = std::min(lo, e);
            hi = std::max(hi, e);
        }
        check(hi > lo * 3.0,
              "sampler filter LFO sweeps the cutoff, modulating the high band over time");
        check(audio::Sampler().filterLfoDepth() == 0.0f, "sampler filter LFO defaults to off");

        // Tempo sync locks the LFO rate to the division: 1/8 @120 BPM = 4 Hz, 1/16 = 8 Hz. With sync
        // off, updateTempo leaves the manual rate alone.
        audio::Sampler ts;
        ts.setFilterLfo(5.0f, 6000.0f);
        ts.updateTempo(120.0); // sync off → no change
        check(std::fabs(ts.filterLfoRate() - 5.0f) < 1e-3f, "sampler LFO ignores tempo when unsynced");
        ts.setFilterLfoSync(true);
        ts.setFilterLfoSyncDivision(3); // 1/8
        ts.updateTempo(120.0);
        check(std::fabs(ts.filterLfoRate() - 4.0f) < 0.05f, "sampler LFO 1/8 @120 BPM = 4 Hz");
        ts.setFilterLfoSyncDivision(5); // 1/16
        ts.updateTempo(120.0);
        check(std::fabs(ts.filterLfoRate() - 8.0f) < 0.05f, "sampler LFO 1/16 @120 BPM = 8 Hz");
        check(!audio::Sampler().filterLfoSync(), "sampler filter LFO sync defaults to off");

        // LFO shape: a square gives a hard bang-bang sweep, distinct from the smooth sine, but still
        // gates the high band over time.
        audio::Sampler sq;
        sq.setSampleMono(two, sr);
        sq.setBasePitch(60);
        sq.setKeyTrack(false);
        sq.setFilter(3000.0f, 0.7f);
        sq.setFilterLfo(4.0f, 6000.0f);
        sq.setFilterLfoShape(audio::Sampler::LfoShape::Square);
        sq.noteOn(60, 1.0f);
        const std::vector<float> sqOut = renderMono(sq, fn, sr);
        check(sqOut != swept, "sampler filter LFO shape changes the sweep (square != sine)");
        double slo = 1e30, shi = 0.0;
        for (int k = 0; k < 4; ++k) {
            std::vector<float> win(sqOut.begin() + static_cast<long>(k) * w,
                                   sqOut.begin() + static_cast<long>(k + 1) * w);
            const double e = goertzel(win, 6000.0, sr);
            slo = std::min(slo, e);
            shi = std::max(shi, e);
        }
        check(shi > slo * 3.0, "square filter LFO still gates the high band over time");
        check(audio::Sampler().filterLfoShape() == audio::Sampler::LfoShape::Sine,
              "sampler filter LFO shape defaults to sine");
    }

    // --- WAV reader: 24-bit PCM and 32-bit float support ---------------------
    {
        auto putLE = [](std::vector<uint8_t>& v, uint32_t x, int n) {
            for (int k = 0; k < n; ++k) {
                v.push_back(static_cast<uint8_t>((x >> (8 * k)) & 0xFFu));
            }
        };
        auto writeRaw = [&](const std::string& p, const std::vector<float>& s, int bits, int fmt) {
            const int ch = 1, sr2 = 48000, bytesPer = bits / 8;
            std::vector<uint8_t> data;
            for (float fv : s) {
                if (fmt == 3) { // 32-bit float
                    uint32_t u;
                    std::memcpy(&u, &fv, 4);
                    putLE(data, u, 4);
                } else if (bits == 24) {
                    const int32_t q = static_cast<int32_t>(fv * 8388607.0f);
                    putLE(data, static_cast<uint32_t>(q) & 0xFFFFFFu, 3);
                } else if (bits == 32) {
                    putLE(data, static_cast<uint32_t>(static_cast<int32_t>(fv * 2147483647.0f)), 4);
                } // bits == 8 → no data written (an unsupported format for the reject test)
            }
            std::vector<uint8_t> bb;
            bb.insert(bb.end(), {'R', 'I', 'F', 'F'});
            putLE(bb, 36u + static_cast<uint32_t>(data.size()), 4);
            bb.insert(bb.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
            putLE(bb, 16, 4);
            putLE(bb, static_cast<uint32_t>(fmt), 2);
            putLE(bb, static_cast<uint32_t>(ch), 2);
            putLE(bb, static_cast<uint32_t>(sr2), 4);
            putLE(bb, static_cast<uint32_t>(sr2 * ch * bytesPer), 4);
            putLE(bb, static_cast<uint32_t>(ch * bytesPer), 2);
            putLE(bb, static_cast<uint32_t>(bits), 2);
            bb.insert(bb.end(), {'d', 'a', 't', 'a'});
            putLE(bb, static_cast<uint32_t>(data.size()), 4);
            bb.insert(bb.end(), data.begin(), data.end());
            std::ofstream of(p, std::ios::binary);
            of.write(reinterpret_cast<const char*>(bb.data()), static_cast<std::streamsize>(bb.size()));
        };
        const std::vector<float> ref = {0.5f, -0.25f, 0.75f, -0.9f};

        writeRaw("unit_wav24.wav", ref, 24, 1);
        audio::WavData w24;
        check(audio::readWav16("unit_wav24.wav", w24, &err), "24-bit WAV reads");
        bool ok24 = w24.samples.size() == ref.size();
        for (size_t i = 0; ok24 && i < ref.size(); ++i) {
            ok24 = std::fabs(w24.samples[i] - ref[i]) < 1e-4f;
        }
        check(ok24, "24-bit samples decode to the right values");

        writeRaw("unit_wavf32.wav", ref, 32, 3);
        audio::WavData wf;
        check(audio::readWav16("unit_wavf32.wav", wf, &err), "32-bit float WAV reads");
        bool okf = wf.samples.size() == ref.size();
        for (size_t i = 0; okf && i < ref.size(); ++i) {
            okf = std::fabs(wf.samples[i] - ref[i]) < 1e-6f;
        }
        check(okf, "32-bit float samples decode exactly");

        writeRaw("unit_wav8.wav", ref, 8, 1);
        audio::WavData w8;
        check(!audio::readWav16("unit_wav8.wav", w8, &err), "an unsupported bit depth is rejected");
    }

    // Missing file fails cleanly.
    audio::Sampler bad;
    check(!bad.load("/nonexistent/missing.wav", &err), "loading a missing WAV fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
