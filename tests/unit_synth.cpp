// Unit tests for the A2 melodic path — pitch math, the polyphonic SynthInstrument, the PianoRoll
// note model, and melodic scheduling through the Sequencer. Pure DSP/logic, no audio device.

#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/Pitch.hpp"
#include "maz/audio/Sequencer.hpp"
#include "maz/audio/SynthInstrument.hpp"

#include <cmath>
#include <cstdio>
#include <string>
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

double rms(const std::vector<float>& buf) {
    if (buf.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float s : buf) {
        sum += static_cast<double>(s) * static_cast<double>(s);
    }
    return std::sqrt(sum / static_cast<double>(buf.size()));
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

std::vector<float> render(audio::SynthInstrument& synth, int frames, int sampleRate) {
    std::vector<float> buf(static_cast<size_t>(frames), 0.0f);
    synth.render(buf.data(), frames, sampleRate);
    return buf;
}

} // namespace

int main() {
    const int sampleRate = 48000;

    // --- Pitch math ----------------------------------------------------------
    check(std::fabs(audio::midiToFreq(69) - 440.0f) < 0.01f, "MIDI 69 == 440 Hz (A4)");
    check(std::fabs(audio::midiToFreq(60) - 261.63f) < 0.5f, "MIDI 60 ~= 261.6 Hz (middle C)");
    check(std::string(audio::pitchClassName(60)) == "C", "MIDI 60 names as C");
    check(audio::midiOctave(60) == 4, "MIDI 60 is octave 4");

    // --- SynthInstrument polyphony + pitch ----------------------------------
    audio::SynthInstrument synth;
    synth.setWaveform(audio::Waveform::Sine);
    synth.setEnvelope(0.002f, 0.02f, 0.9f, 0.05f);
    check(!synth.active(), "synth starts idle");

    synth.noteOn(69, 1.0f); // A4 = 440 Hz
    check(synth.active(), "synth active after noteOn");
    const std::vector<float> tone = render(synth, sampleRate / 4, sampleRate); // 0.25 s of sustain
    check(rms(tone) > 0.0, "held note produces sound");
    check(std::fabs(estimateHz(tone, sampleRate) - 440.0) < 3.0, "note plays at the right pitch");

    // A second, simultaneous note → louder than one alone (polyphony sums).
    audio::SynthInstrument poly;
    poly.setEnvelope(0.002f, 0.02f, 0.9f, 0.05f);
    poly.noteOn(60, 1.0f);
    const double oneVoice = rms(render(poly, 4800, sampleRate));
    poly.noteOn(64, 1.0f);
    const double twoVoices = rms(render(poly, 4800, sampleRate));
    check(twoVoices > oneVoice, "two simultaneous notes are louder than one");

    // noteOff → releases to silence and goes idle.
    synth.noteOff(69);
    const std::vector<float> release = render(synth, sampleRate, sampleRate); // 1 s
    check(!synth.active(), "synth goes idle after note release");
    check(std::fabs(release.back()) < 1e-4f, "released note decays to silence");

    // --- FM engine -----------------------------------------------------------
    // FM keeps the carrier's fundamental pitch but produces a different (brighter) waveform, so its
    // output differs from the subtractive engine for the same note.
    audio::SynthInstrument sub;
    sub.setMode(audio::SynthMode::Subtractive);
    sub.setWaveform(audio::Waveform::Sine);
    sub.setEnvelope(0.002f, 0.02f, 0.9f, 0.05f);
    sub.noteOn(69, 1.0f);
    const std::vector<float> subOut = render(sub, sampleRate / 4, sampleRate);

    audio::SynthInstrument fm;
    fm.setMode(audio::SynthMode::FM);
    fm.setFmRatio(2.0f);
    fm.setFmIndex(5.0f);
    fm.setEnvelope(0.002f, 0.02f, 0.9f, 0.05f);
    fm.noteOn(69, 1.0f);
    const std::vector<float> fmOut = render(fm, sampleRate / 4, sampleRate);

    check(rms(fmOut) > 0.0, "FM engine produces sound");
    // FM adds harmonics, so zero-crossings overcount; verify the fundamental via autocorrelation
    // (the lag of peak self-similarity is the period). Expected ~109 samples (48000/440).
    {
        int bestLag = 0;
        double best = -1.0;
        for (int lag = 60; lag < 300; ++lag) {
            double acc = 0.0;
            for (size_t i = 0; i + static_cast<size_t>(lag) < fmOut.size(); ++i) {
                acc += static_cast<double>(fmOut[i]) * static_cast<double>(fmOut[i + static_cast<size_t>(lag)]);
            }
            if (acc > best) {
                best = acc;
                bestLag = lag;
            }
        }
        const double f = static_cast<double>(sampleRate) / bestLag;
        check(std::fabs(f - 440.0) < 12.0, "FM keeps the carrier pitch (autocorrelation)");
    }
    double diff = 0.0;
    for (size_t i = 0; i < fmOut.size(); ++i) {
        diff += std::fabs(static_cast<double>(fmOut[i] - subOut[i]));
    }
    check(diff > 1.0, "FM output differs from subtractive for the same note");

    // --- FM feedback: operator self-feedback adds harmonics ------------------
    {
        auto fmBright = [&](float feedback) {
            audio::SynthInstrument s;
            s.setMode(audio::SynthMode::FM);
            s.setFmRatio(1.0f);
            s.setFmIndex(1.0f); // modest base brightness so feedback's effect is clear
            s.setFmFeedback(feedback);
            s.setEnvelope(0.002f, 0.02f, 1.0f, 0.05f);
            s.noteOn(57, 1.0f);
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            double h = 0.0, en = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
                en += static_cast<double>(out[i]) * out[i];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        check(fmBright(0.9f) > fmBright(0.0f) * 1.3, "FM feedback adds high-frequency harmonics");
        audio::SynthInstrument dfb;
        check(dfb.fmFeedback() == 0.0f, "FM feedback defaults to 0");
    }

    // --- Oscillator section: detune / sub / noise ----------------------------
    {
        audio::SynthInstrument single;
        single.setWaveform(audio::Waveform::Saw);
        single.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        single.noteOn(57, 1.0f); // A3 = 220 Hz
        const std::vector<float> singleOut = render(single, sampleRate / 4, sampleRate);

        // A detuned 2nd oscillator makes the output differ (beating/width).
        audio::SynthInstrument fat;
        fat.setWaveform(audio::Waveform::Saw);
        fat.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        fat.setOscillators(20.0f, 0.8f, 0.0f, 0.0f);
        fat.noteOn(57, 1.0f);
        const std::vector<float> fatOut = render(fat, sampleRate / 4, sampleRate);
        double oscDiff = 0.0;
        for (size_t i = 0; i < fatOut.size(); ++i) {
            oscDiff += std::fabs(static_cast<double>(fatOut[i] - singleOut[i]));
        }
        check(oscDiff > 1.0, "a detuned 2nd oscillator changes the sound");

        // Noise adds broadband (high-frequency) energy.
        audio::SynthInstrument noisy;
        noisy.setWaveform(audio::Waveform::Sine);
        noisy.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        noisy.setOscillators(0.0f, 0.0f, 0.0f, 0.6f);
        noisy.noteOn(57, 1.0f);
        const std::vector<float> noisyOut = render(noisy, sampleRate / 4, sampleRate);
        auto hf = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                s += d * d;
            }
            return s;
        };
        check(hf(noisyOut) > hf(singleOut), "noise layer adds high-frequency energy");

        // Sub-oscillator waveform: a square sub adds harmonics vs the default sine sub.
        auto subRender = [&](audio::Waveform sw) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 0.0f, 0.8f, 0.0f); // sub only (plus the sine carrier)
            s.setSubWaveform(sw);
            s.noteOn(57, 1.0f);
            return render(s, sampleRate / 4, sampleRate);
        };
        check(hf(subRender(audio::Waveform::Square)) > hf(subRender(audio::Waveform::Sine)),
              "a square sub-oscillator adds harmonics vs a sine sub");
        audio::SynthInstrument dsub;
        check(dsub.subWaveform() == audio::Waveform::Sine, "sub waveform defaults to sine");

        // Noise color: darkening the noise removes high-frequency energy.
        auto noiseRender = [&](float color) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 0.0f, 0.0f, 0.8f); // noise-heavy
            s.setNoiseColor(color);
            s.noteOn(57, 1.0f);
            return render(s, sampleRate / 4, sampleRate);
        };
        check(hf(noiseRender(1.0f)) < hf(noiseRender(0.0f)) * 0.6,
              "darkening the noise color removes high-frequency energy");
        audio::SynthInstrument dn;
        check(dn.noiseColor() == 0.0f, "noise color defaults to white (0)");

        // Hard sync: the 2nd oscillator (a sine, same pitch when unsynced) resets every master cycle
        // when synced, injecting harmonics → much more high-frequency energy.
        auto syncRender = [&](bool on) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 1.0f, 0.0f, 0.0f); // osc2 fully up, no detune
            s.setHardSync(on);
            s.setSyncRatio(4.0f);
            s.noteOn(60, 1.0f); // C4
            return render(s, sampleRate / 4, sampleRate);
        };
        const std::vector<float> syncOff = syncRender(false);
        const std::vector<float> syncOn = syncRender(true);
        check(hf(syncOn) > hf(syncOff) * 2.0, "hard sync injects high-frequency harmonics");
        double syncDiff = 0.0;
        for (size_t i = 0; i < syncOn.size(); ++i) {
            syncDiff += std::fabs(static_cast<double>(syncOn[i] - syncOff[i]));
        }
        check(syncDiff > 1.0, "hard sync changes the sound");
        audio::SynthInstrument dsync;
        check(!dsync.hardSync(), "hard sync defaults off");

        // Osc2 coarse tune: a 2nd sine oscillator an octave up adds a bright partial the unison
        // (coarse 0) tone lacks → more high-frequency energy.
        auto coarseRender = [&](float semis) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 1.0f, 0.0f, 0.0f); // osc2 fully up, no fine detune
            s.setOsc2Semitones(semis);
            s.noteOn(48, 1.0f); // C3 (low, so the raised partials stay well below Nyquist)
            return render(s, sampleRate / 4, sampleRate);
        };
        // Both cases have osc2 at full level; only the interval differs, so the extra high-frequency
        // energy comes purely from the coarse tune raising osc2's pitch (+24 = two octaves).
        check(hf(coarseRender(24.0f)) > hf(coarseRender(0.0f)) * 2.0,
              "a coarse-tuned osc2 (two octaves up) adds high-frequency energy");
        audio::SynthInstrument dcoarse;
        check(dcoarse.osc2Semitones() == 0.0f, "osc2 coarse tune defaults to 0");

        // Pulse width: the fraction of a square oscillator's samples spent high equals the duty cycle.
        auto positiveFraction = [&](float pulseWidth) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Square);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 0.0f, 0.0f, 0.0f); // primary square only
            s.setPulseWidth(pulseWidth);
            s.noteOn(57, 1.0f); // A3 = 220 Hz
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            size_t pos = 0, total = 0;
            // Skip the first few ms (attack ramp) so we measure the steady pulse.
            for (size_t i = static_cast<size_t>(sampleRate) / 100; i < out.size(); ++i) {
                if (out[i] > 0.0f) ++pos;
                ++total;
            }
            return static_cast<double>(pos) / static_cast<double>(total);
        };
        check(std::fabs(positiveFraction(0.5f) - 0.5) < 0.05, "a 0.5 pulse width is a 50% duty square");
        check(std::fabs(positiveFraction(0.25f) - 0.25) < 0.05, "a 0.25 pulse width narrows the duty to 25%");
        audio::SynthInstrument dpw;
        check(std::fabs(dpw.pulseWidth() - 0.5f) < 1e-6f, "pulse width defaults to 0.5 (square)");
    }

    // --- Resonant filter -----------------------------------------------------
    {
        // A saw through a low cutoff loses most of its high-harmonic energy vs. an open filter.
        audio::SynthInstrument open;
        open.setWaveform(audio::Waveform::Saw);
        open.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        open.setFilter(20000.0f, 0.7f, 0.0f); // bypassed
        open.noteOn(60, 1.0f);
        const std::vector<float> openOut = render(open, sampleRate / 4, sampleRate);

        audio::SynthInstrument closed;
        closed.setWaveform(audio::Waveform::Saw);
        closed.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        closed.setFilter(300.0f, 0.7f, 0.0f); // low cutoff
        closed.noteOn(60, 1.0f);
        const std::vector<float> closedOut = render(closed, sampleRate / 4, sampleRate);

        // "Brightness" ~ high-frequency energy via first-difference RMS.
        auto hf = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                s += d * d;
            }
            return std::sqrt(s / static_cast<double>(b.size()));
        };
        check(hf(closedOut) < hf(openOut) * 0.5, "low cutoff removes high-frequency energy");
        check(rms(closedOut) > 0.0, "filtered synth still produces sound");

        // Velocity → cutoff: a hard note opens the filter further than a soft one, so it is brighter
        // (independent of loudness — brightness is measured as HF energy over total energy).
        auto velBright = [&](float velocity) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setFilter(300.0f, 0.7f, 0.0f); // low base cutoff so the mod is audible
            s.setVelToCutoff(6000.0f);
            s.noteOn(57, velocity);
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            double h = 0.0, en = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
                en += static_cast<double>(out[i]) * out[i];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        check(velBright(1.0f) > velBright(0.3f) * 1.3,
              "velocity opens the filter (harder notes are brighter)");
        audio::SynthInstrument dvc;
        check(dvc.velToCutoff() == 0.0f, "velocity→cutoff defaults to 0");
    }

    // --- Wavetable synthesis -------------------------------------------------
    {
        auto hf = [](const std::vector<float>& b) {
            double s = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i] - b[i - 1]);
                s += d * d;
            }
            return std::sqrt(s / static_cast<double>(b.size()));
        };

        // Position 0 is the pure-sine frame; position 1 is the square frame. Scanning the table
        // toward 1 must add high-frequency (harmonic) energy — the defining wavetable behaviour.
        audio::SynthInstrument dark;
        dark.setMode(audio::SynthMode::Wavetable);
        dark.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        dark.setFilter(20000.0f, 0.7f, 0.0f); // keep the filter out of the way
        dark.setWavetablePosition(0.0f);
        dark.noteOn(57, 1.0f);
        const std::vector<float> darkOut = render(dark, sampleRate / 4, sampleRate);

        audio::SynthInstrument bright;
        bright.setMode(audio::SynthMode::Wavetable);
        bright.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        bright.setFilter(20000.0f, 0.7f, 0.0f);
        bright.setWavetablePosition(1.0f);
        bright.noteOn(57, 1.0f);
        const std::vector<float> brightOut = render(bright, sampleRate / 4, sampleRate);

        check(rms(darkOut) > 0.0, "wavetable synth produces sound");
        check(hf(brightOut) > hf(darkOut) * 2.0,
              "scanning the wavetable toward the square frame adds harmonics");

        // Position 0 (sine frame) should track the note pitch cleanly.
        check(std::fabs(estimateHz(darkOut, sampleRate) - audio::midiToFreq(57)) < 5.0,
              "wavetable sine frame holds the note pitch");

        // Direct table sanity: the default table's first frame is a sine (zero at phase 0).
        audio::Wavetable wt;
        check(std::fabs(wt.sample(0.0f, 0.0)) < 1e-3f, "wavetable frame 0 phase 0 ~= 0 (sine)");
        check(wt.sample(0.0f, 0.25) > 0.9f, "wavetable frame 0 quarter-phase ~= +1 (sine peak)");

        // Custom frames: making frame 0 a square (instead of the default sine) makes position 0
        // bright rather than dark — more high-frequency energy at the same scan position.
        audio::SynthInstrument custom;
        custom.setMode(audio::SynthMode::Wavetable);
        custom.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        custom.setFilter(20000.0f, 0.7f, 0.0f);
        custom.setWavetablePosition(0.0f);
        custom.setWavetableFrames(audio::Waveform::Square, audio::Waveform::Square,
                                  audio::Waveform::Square, audio::Waveform::Square);
        custom.noteOn(57, 1.0f);
        const std::vector<float> squareFrame = render(custom, sampleRate / 4, sampleRate);
        check(hf(squareFrame) > hf(darkOut) * 2.0,
              "a square first frame makes position 0 brighter than the default sine");
        check(custom.wavetableFrame(0) == audio::Waveform::Square, "custom wavetable frame is stored");
    }

    // --- Unison (supersaw) ---------------------------------------------------
    {
        // Ratio of loudest to quietest short-window RMS across the render: detuned unison voices
        // beat against each other, so the amplitude swells and dips; a single voice is steady.
        auto beating = [&](int voices, float detune) {
            audio::SynthInstrument syn;
            syn.setWaveform(audio::Waveform::Saw);
            syn.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            syn.setFilter(20000.0f, 0.7f, 0.0f); // bypass filter
            syn.setUnison(voices, detune);
            syn.noteOn(57, 1.0f);
            const std::vector<float> out = render(syn, sampleRate, sampleRate); // 1 s
            const int win = 2000;
            double lo = 1e9, hi = 0.0;
            for (size_t base = 0; base + static_cast<size_t>(win) < out.size();
                 base += static_cast<size_t>(win)) {
                double e = 0.0;
                for (int i = 0; i < win; ++i) {
                    const double v = static_cast<double>(out[base + static_cast<size_t>(i)]);
                    e += v * v;
                }
                e = std::sqrt(e / win);
                lo = std::min(lo, e);
                hi = std::max(hi, e);
            }
            return lo > 1e-9 ? hi / lo : 1.0;
        };

        const double single = beating(1, 0.0f);
        const double superSaw = beating(7, 25.0f);
        check(superSaw > single * 1.5, "unison detuning produces amplitude beating (supersaw)");
        check(std::abs(single - 1.0) < 0.3, "a single voice is comparatively steady");

        audio::SynthInstrument def;
        check(def.unisonVoices() == 1, "unison defaults to a single voice");
    }

    // --- Pitch envelope ------------------------------------------------------
    {
        // A note with a +12-semitone pitch env starting fast-decaying: the onset reads about an
        // octave sharp, the settled tail reads the written pitch.
        audio::SynthInstrument pe;
        pe.setWaveform(audio::Waveform::Saw);
        pe.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        pe.setFilter(20000.0f, 0.7f, 0.0f);
        pe.setPitchEnv(12.0f, 0.03f); // +1 octave, ~30 ms decay
        pe.noteOn(57, 1.0f);          // A3 = 220 Hz target
        const std::vector<float> out = render(pe, sampleRate, sampleRate);
        auto hzIn = [&](int start, int len) {
            int c = 0;
            for (int i = start + 1; i < start + len; ++i) {
                if (out[static_cast<size_t>(i - 1)] <= 0.0f && out[static_cast<size_t>(i)] > 0.0f) {
                    ++c;
                }
            }
            return static_cast<double>(c) * sampleRate / len;
        };
        const double onset = hzIn(0, 600);        // first ~12 ms → sharp
        const double settled = hzIn(20000, 6000); // after the env has decayed → written pitch
        check(onset > settled * 1.4, "pitch env starts the note sharp");
        check(std::fabs(settled - 220.0) < 8.0, "pitch env settles to the written pitch");

        audio::SynthInstrument d;
        check(d.pitchEnvAmount() == 0.0f, "pitch env defaults to off");
    }

    // --- Vibrato (pitch LFO) -------------------------------------------------
    {
        // A slow, deep vibrato: measure the pitch in a window near the LFO's positive peak vs. its
        // negative peak — the peak window should read sharp (higher) and the trough flat (lower).
        audio::SynthInstrument vib;
        vib.setWaveform(audio::Waveform::Saw);
        vib.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        vib.setFilter(20000.0f, 0.7f, 0.0f);
        vib.setVibrato(2.0f, 80.0f); // 2 Hz, ±80 cents
        vib.noteOn(69, 1.0f);        // A4 = 440 Hz
        const std::vector<float> out = render(vib, sampleRate, sampleRate); // 1 s
        auto hzIn = [&](int start, int len) {
            int crossings = 0;
            for (int i = start + 1; i < start + len; ++i) {
                if (out[static_cast<size_t>(i - 1)] <= 0.0f && out[static_cast<size_t>(i)] > 0.0f) {
                    ++crossings;
                }
            }
            return static_cast<double>(crossings) * sampleRate / len;
        };
        // LFO period = 0.5 s (24000 frames). Peak of sin near t=0.125 s (frame 6000), trough near
        // t=0.375 s (frame 18000). Measure short windows there.
        const double sharp = hzIn(4000, 4000);  // around the +peak → higher pitch
        const double flat = hzIn(16000, 4000);   // around the −peak → lower pitch
        check(sharp > flat + 5.0, "vibrato wavers the pitch (sharp at the LFO peak, flat at trough)");

        // No vibrato → steady pitch across the same windows.
        audio::SynthInstrument steady;
        steady.setWaveform(audio::Waveform::Saw);
        steady.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        steady.setFilter(20000.0f, 0.7f, 0.0f);
        steady.noteOn(69, 1.0f);
        const std::vector<float> so = render(steady, sampleRate, sampleRate);
        auto hzIn2 = [&](int start, int len) {
            int c = 0;
            for (int i = start + 1; i < start + len; ++i) {
                if (so[static_cast<size_t>(i - 1)] <= 0.0f && so[static_cast<size_t>(i)] > 0.0f) {
                    ++c;
                }
            }
            return static_cast<double>(c) * sampleRate / len;
        };
        check(std::fabs(hzIn2(4000, 4000) - hzIn2(16000, 4000)) < 4.0,
              "without vibrato the pitch is steady");
        audio::SynthInstrument dv;
        check(dv.vibratoDepth() == 0.0f, "vibrato depth defaults to 0 (off)");
    }

    // --- Portamento / glide --------------------------------------------------
    {
        // Play a low note, release, then a high note with glide on. Early in the second note the
        // pitch should still be well below its 880 Hz target; later it should have arrived.
        audio::SynthInstrument glider;
        glider.setWaveform(audio::Waveform::Saw);
        glider.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        glider.setGlide(0.20f); // 200 ms glide
        glider.noteOn(57, 1.0f); // A3 = 220 Hz — establishes lastFreq
        (void)render(glider, sampleRate / 20, sampleRate);
        glider.noteOff(57);
        (void)render(glider, sampleRate / 20, sampleRate);

        glider.noteOn(81, 1.0f); // A5 = 880 Hz target, glides up from ~220
        const std::vector<float> early = render(glider, sampleRate / 100, sampleRate); // first 10 ms
        (void)render(glider, sampleRate, sampleRate); // let the one-pole glide settle (~1 s >> 200 ms)
        const std::vector<float> late = render(glider, sampleRate / 10, sampleRate); // steady 100 ms
        const double earlyHz = estimateHz(early, sampleRate);
        const double lateHz = estimateHz(late, sampleRate);
        check(earlyHz < 700.0, "glide starts the note below its target pitch");
        check(lateHz > 840.0, "glide arrives near the target pitch after settling");
        check(lateHz > earlyHz + 100.0, "glide sweeps the pitch upward over time");

        // With glide off, the note is on-pitch immediately.
        audio::SynthInstrument instant;
        instant.setWaveform(audio::Waveform::Saw);
        instant.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        instant.noteOn(57, 1.0f);
        (void)render(instant, sampleRate / 20, sampleRate);
        instant.noteOff(57);
        (void)render(instant, sampleRate / 20, sampleRate);
        instant.noteOn(81, 1.0f);
        const std::vector<float> imm = render(instant, sampleRate / 20, sampleRate);
        check(std::fabs(estimateHz(imm, sampleRate) - 880.0) < 30.0,
              "with glide off the note plays its pitch immediately");
    }

    // --- PianoRoll model -----------------------------------------------------
    audio::PianoRoll roll;
    check(roll.notes().empty(), "roll starts empty");
    roll.toggle(60, 0);
    check(roll.hasNote(60, 0) && roll.notes().size() == 1, "toggle adds a note");
    roll.toggle(60, 0);
    check(!roll.hasNote(60, 0) && roll.notes().empty(), "toggling again removes it");

    // Quantize: snap note starts to the nearest multiple of the division.
    {
        audio::PianoRoll qr;
        qr.addNote(audio::Note{3, 2, 60, 1.0f});  // → nearest 4 = 4
        qr.addNote(audio::Note{5, 2, 62, 1.0f});  // → nearest 4 = 4
        qr.addNote(audio::Note{8, 2, 64, 1.0f});  // already on grid → unchanged
        const int moved = qr.quantize(4);
        check(moved == 2, "quantize moves the off-grid notes only");
        check(qr.notes()[0].startStep == 4 && qr.notes()[1].startStep == 4 &&
                  qr.notes()[2].startStep == 8,
              "quantize snaps starts to the nearest division");
        check(qr.quantize(1) == 0, "quantize to 1 is a no-op");
    }

    // --- Chord tool ----------------------------------------------------------
    {
        audio::PianoRoll cr;
        // C major at C4 (60) → C E G = 60, 64, 67, all starting at step 2 for 4 steps.
        const int added = cr.addChord(2, 4, 60, audio::Chord::Major, 0.8f);
        check(added == 3 && cr.notes().size() == 3, "major chord adds three notes");
        check(cr.hasNote(60, 2) && cr.hasNote(64, 2) && cr.hasNote(67, 2),
              "major chord has root, major third, and fifth");
        for (const audio::Note& n : cr.notes()) {
            check(n.lengthSteps == 4 && std::fabs(n.velocity - 0.8f) < 1e-4f,
                  "chord notes share the given length and velocity");
        }

        // Minor uses a flat third; a dominant 7th adds a fourth note.
        audio::PianoRoll mr;
        mr.addChord(0, 1, 57, audio::Chord::Minor); // A minor: A C E = 57, 60, 64
        check(mr.hasNote(57, 0) && mr.hasNote(60, 0) && mr.hasNote(64, 0), "minor chord uses a flat third");
        audio::PianoRoll d7;
        check(d7.addChord(0, 1, 60, audio::Chord::Dom7) == 4, "a dominant 7th is four notes");
        check(d7.hasNote(70, 0), "dominant 7th adds the flat seventh (Bb above C)");
    }

    // --- Scale snap ----------------------------------------------------------
    {
        // C major degrees (pitch classes): 0 2 4 5 7 9 11. Off-scale notes should snap to the
        // nearest degree; in-scale notes stay put.
        audio::PianoRoll sr;
        sr.addNote(audio::Note{0, 1, 61, 1.0f}); // C#4 → nearest is C4 (60, down) or D4 (62, up); tie → down
        sr.addNote(audio::Note{1, 1, 66, 1.0f}); // F#4 → G4 (67, up) is nearer than F4 (65)? both dist 1 → down = F4 65
        sr.addNote(audio::Note{2, 1, 64, 1.0f}); // E4 already in C major → unchanged
        const int moved = sr.snapToScale(60, audio::Scale::Major);
        check(moved == 2, "scale snap moves only the off-scale notes");
        // Every note now belongs to C major.
        const int major[] = {0, 2, 4, 5, 7, 9, 11};
        for (const audio::Note& note : sr.notes()) {
            const int pc = ((note.pitch - 60) % 12 + 12) % 12;
            bool ok = false;
            for (int d : major) {
                if (d == pc) ok = true;
            }
            check(ok, "every snapped note is a C-major scale degree");
        }
        check(sr.notes()[0].pitch == 60, "C# snaps down to C on a tie");
        check(sr.notes()[1].pitch == 65, "F# snaps down to F on a tie");
        check(sr.notes()[2].pitch == 64, "an already in-scale note is untouched");

        // A pentatonic scale is sparser: B4 (71) is not in C major pentatonic (0 2 4 7 9) → snaps.
        audio::PianoRoll pr;
        pr.addNote(audio::Note{0, 1, 71, 1.0f}); // B4, pc 11 → nearest penta degree is 9 (A, down 2) or 0/12 (C5, up 1)
        pr.snapToScale(60, audio::Scale::PentatonicMajor);
        check(pr.notes()[0].pitch == 72, "B snaps up to C in C-major pentatonic (nearest degree)");
    }

    // --- Strum ---------------------------------------------------------------
    {
        // A C-major triad stacked on one step, plus a lone note elsewhere.
        audio::PianoRoll sr;
        sr.addChord(4, 2, 60, audio::Chord::Major); // 60, 64, 67 all at step 4
        sr.addNote(audio::Note{10, 1, 72, 1.0f});   // a single note at step 10 (untouched)
        const int moved = sr.strum(1);
        check(moved == 2, "strum staggers all but the lowest note of each stack");
        // The triad now rolls: C stays at 4, E at 5, G at 6 (low→high).
        check(sr.hasNote(60, 4) && sr.hasNote(64, 5) && sr.hasNote(67, 6),
              "strum rolls the chord low-to-high by the step offset");
        // The lone note is unaffected (not part of a stack).
        check(sr.hasNote(72, 10), "strum leaves single notes in place");

        // A negative offset rolls from the top and clamps starts at 0.
        audio::PianoRoll dn;
        dn.addChord(0, 1, 60, audio::Chord::Major); // 60, 64, 67 at step 0
        dn.strum(-1);
        check(dn.hasNote(60, 0) && dn.hasNote(64, 0) && dn.hasNote(67, 0),
              "a negative strum clamps note starts at 0");
    }

    // --- Legato --------------------------------------------------------------
    {
        audio::PianoRoll lr;
        lr.addNote(audio::Note{0, 1, 60, 1.0f}); // → next start 4 → len 4
        lr.addNote(audio::Note{4, 1, 62, 1.0f}); // → next start 8 → len 4
        lr.addNote(audio::Note{8, 2, 64, 1.0f}); // last note → unchanged (len 2)
        const int changed = lr.legato();
        check(changed == 2, "legato extends every note that has a successor");
        check(lr.notes()[0].lengthSteps == 4 && lr.notes()[1].lengthSteps == 4,
              "legato stretches each note up to the next note's start");
        check(lr.notes()[2].lengthSteps == 2, "the final note keeps its length");

        // A chord: both notes on a stack extend to the next distinct start.
        audio::PianoRoll cr;
        cr.addChord(0, 1, 60, audio::Chord::Major); // 60, 64, 67 at step 0
        cr.addNote(audio::Note{6, 1, 72, 1.0f});    // next start 6
        cr.legato();
        for (const audio::Note& n : cr.notes()) {
            if (n.startStep == 0) {
                check(n.lengthSteps == 6, "every note of a chord stretches to the next start");
            }
        }
    }

    // --- Invert (melodic inversion) ------------------------------------------
    {
        audio::PianoRoll ir;
        ir.addNote(audio::Note{0, 1, 60, 1.0f}); // on the pivot → unchanged
        ir.addNote(audio::Note{1, 1, 64, 1.0f}); // +4 above → +4 below = 56
        ir.addNote(audio::Note{2, 1, 55, 1.0f}); // -5 below → -5 above = 65
        const int changed = ir.invert(60);
        check(changed == 2, "invert mirrors every note off the pivot");
        check(ir.notes()[0].pitch == 60, "a note on the pivot is unchanged");
        check(ir.notes()[1].pitch == 56, "a note above the pivot mirrors below it");
        check(ir.notes()[2].pitch == 65, "a note below the pivot mirrors above it");

        // Inverting twice around the same pivot restores the original pitches.
        ir.invert(60);
        check(ir.notes()[1].pitch == 64 && ir.notes()[2].pitch == 55,
              "inverting twice around the same pivot is a round trip");
    }

    // --- Reverse time (flip horizontally) ------------------------------------
    {
        audio::PianoRoll rr; // default 16-step window
        rr.addNote(audio::Note{0, 4, 60, 1.0f});  // → 16 - 0 - 4 = 12
        rr.addNote(audio::Note{12, 4, 64, 1.0f}); // → 16 - 12 - 4 = 0
        rr.addNote(audio::Note{2, 1, 67, 1.0f});  // → 16 - 2 - 1 = 13
        rr.reverseTime();
        check(rr.hasNote(60, 12) && rr.hasNote(64, 0) && rr.hasNote(67, 13),
              "reverse mirrors note positions in time (pitch/length kept)");
        // Reversing again restores the original timing.
        rr.reverseTime();
        check(rr.hasNote(60, 0) && rr.hasNote(64, 12) && rr.hasNote(67, 2),
              "reversing twice restores the original timing");
    }

    // --- Randomize (humanize) velocities -------------------------------------
    {
        auto build = [](audio::PianoRoll& p) {
            for (int i = 0; i < 8; ++i) {
                p.addNote(audio::Note{i, 1, 60, 0.8f});
            }
        };
        audio::PianoRoll a;
        audio::PianoRoll b;
        build(a);
        build(b);
        a.randomizeVelocity(0.4f, 12345u);
        b.randomizeVelocity(0.4f, 12345u); // same seed → identical result

        bool deterministic = true, inRange = true, anyChanged = false;
        for (size_t i = 0; i < a.notes().size(); ++i) {
            const float v = a.notes()[i].velocity;
            if (v != b.notes()[i].velocity) deterministic = false;
            if (v < 0.0f || v > 1.0f) inRange = false;
            if (std::fabs(v - 0.8f) > 1e-6f) anyChanged = true;
        }
        check(deterministic, "randomize is deterministic for a given seed");
        check(inRange, "randomized velocities stay in [0,1]");
        check(anyChanged, "randomize actually varies the velocities");

        // A different seed gives a different outcome.
        audio::PianoRoll c;
        build(c);
        c.randomizeVelocity(0.4f, 999u);
        bool differs = false;
        for (size_t i = 0; i < c.notes().size(); ++i) {
            if (std::fabs(c.notes()[i].velocity - a.notes()[i].velocity) > 1e-6f) differs = true;
        }
        check(differs, "a different seed yields a different randomization");
    }

    // --- Melodic scheduling through the Sequencer ---------------------------
    audio::Sequencer seq;
    seq.setBpm(120.0); // 6000 samples/step @ 48 kHz
    audio::Note n;
    n.startStep = 0;
    n.lengthSteps = 2;
    n.pitch = 72;
    n.velocity = 1.0f;
    seq.roll().addNote(n);
    seq.synth().setEnvelope(0.002f, 0.02f, 0.8f, 0.05f);
    seq.play();

    std::vector<float> firstStep(6000 * 2, 0.0f); // interleaved stereo
    seq.render(firstStep.data(), 6000, sampleRate);
    check(rms(firstStep) > 0.0, "a scheduled note sounds on its start step");

    // The second (bass) instrument: roll2 notes play through synth2.
    audio::Sequencer bass;
    bass.setBpm(120.0);
    bass.roll2().addNote(audio::Note{0, 8, 40, 1.0f});
    bass.synth2().setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
    bass.play();
    std::vector<float> bassOut(6000 * 2, 0.0f);
    bass.render(bassOut.data(), 6000, sampleRate);
    check(rms(bassOut) > 0.0, "second instrument (roll2/synth2) produces sound");

    // An empty roll with no drums stays silent.
    audio::Sequencer quiet;
    quiet.play();
    std::vector<float> nothing(6000 * 2, 0.0f);
    quiet.render(nothing.data(), 6000, sampleRate);
    check(rms(nothing) == 0.0, "empty pattern renders silence");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
