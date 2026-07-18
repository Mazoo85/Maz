// Unit test for the A4 multisampler — the WAV reader and the Sampler's pitch-shifted playback. It
// synthesizes a 220 Hz sine, writes it to a WAV, loads it back, and checks that playing it at the
// base note reproduces 220 Hz while an octave up plays 440 Hz. Pure DSP, no audio device.

#include "maz/audio/Pitch.hpp"
#include "maz/audio/Sampler.hpp"
#include "maz/audio/WavReader.hpp"
#include "maz/audio/WavWriter.hpp"

#include <cmath>
#include <cstdio>
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

    // Directly injected mono sample works too.
    audio::Sampler sampler3;
    sampler3.setSampleMono(sine, sr);
    sampler3.setBasePitch(57);
    sampler3.noteOn(57, 1.0f);
    check(estimateHz(renderMono(sampler3, sr / 5, sr), sr) > 100.0, "injected sample plays");

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

    // Missing file fails cleanly.
    audio::Sampler bad;
    check(!bad.load("/nonexistent/missing.wav", &err), "loading a missing WAV fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
