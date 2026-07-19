// Unit tests for the A2 melodic path — pitch math, the polyphonic SynthInstrument, the PianoRoll
// note model, and melodic scheduling through the Sequencer. Pure DSP/logic, no audio device.

#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/Pitch.hpp"
#include "maz/audio/Sequencer.hpp"
#include "maz/audio/SynthInstrument.hpp"

#include <algorithm>
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

    // --- Filter cutoff LFO tempo sync ---------------------------------------
    {
        audio::SynthInstrument syn;
        check(!syn.filterLfoSync(), "cutoff LFO sync defaults to off");
        syn.setFilterLfo(3.0f, 1.0f); // a free-running rate
        syn.updateTempo(120.0);
        check(std::fabs(syn.filterLfoRate() - 3.0f) < 1e-4f,
              "with sync off, updateTempo leaves the free-running rate alone");

        syn.setFilterLfoSync(true);
        syn.setFilterLfoSyncDivision(3); // 1/8 → 2 cycles per beat
        check(syn.filterLfoSyncDivision() == 3, "cutoff LFO sync division is settable");
        syn.updateTempo(120.0); // 120 BPM → 2 beats/s → 1/8 = 4 Hz
        check(std::fabs(syn.filterLfoRate() - 4.0f) < 1e-3f,
              "1/8 sync at 120 BPM locks the cutoff LFO to 4 Hz");
        syn.updateTempo(60.0); // 60 BPM → 1/8 = 2 Hz
        check(std::fabs(syn.filterLfoRate() - 2.0f) < 1e-3f,
              "the synced cutoff LFO tracks a tempo change");
        syn.setFilterLfoSyncDivision(2); // 1/4 → 1 cycle per beat
        syn.updateTempo(120.0);          // → 2 Hz
        check(std::fabs(syn.filterLfoRate() - 2.0f) < 1e-3f, "1/4 sync at 120 BPM locks to 2 Hz");
    }

    // --- Tremolo (amp LFO) tempo sync ---------------------------------------
    {
        audio::SynthInstrument syn;
        check(!syn.ampLfoSync(), "tremolo LFO sync defaults to off");
        syn.setAmpLfo(5.0f, 0.5f); // a free-running rate + depth
        syn.updateTempo(120.0);
        check(std::fabs(syn.ampLfoRate() - 5.0f) < 1e-4f,
              "with sync off, updateTempo leaves the free-running tremolo rate alone");

        syn.setAmpLfoSync(true);
        syn.setAmpLfoSyncDivision(3); // 1/8 → 4 Hz at 120 BPM
        check(syn.ampLfoSyncDivision() == 3, "tremolo sync division is settable");
        syn.updateTempo(120.0);
        check(std::fabs(syn.ampLfoRate() - 4.0f) < 1e-3f,
              "1/8 tremolo sync at 120 BPM locks to 4 Hz");
        syn.updateTempo(90.0); // 90 BPM → 1/8 = 3 Hz
        check(std::fabs(syn.ampLfoRate() - 3.0f) < 1e-3f, "the synced tremolo tracks a tempo change");
        // The tremolo depth is untouched by the rate sync.
        check(std::fabs(syn.ampLfoDepth() - 0.5f) < 1e-6f, "tremolo sync leaves the depth alone");
    }

    // --- Vibrato (pitch LFO) tempo sync -------------------------------------
    {
        audio::SynthInstrument syn;
        check(!syn.vibratoSync(), "vibrato sync defaults to off");
        syn.setVibrato(6.0f, 30.0f); // free-running rate + depth
        syn.updateTempo(120.0);
        check(std::fabs(syn.vibratoRate() - 6.0f) < 1e-4f,
              "with sync off, updateTempo leaves the free-running vibrato rate alone");

        syn.setVibratoSync(true);
        syn.setVibratoSyncDivision(2); // 1/4 → 2 Hz at 120 BPM
        check(syn.vibratoSyncDivision() == 2, "vibrato sync division is settable");
        syn.updateTempo(120.0);
        check(std::fabs(syn.vibratoRate() - 2.0f) < 1e-3f, "1/4 vibrato sync at 120 BPM locks to 2 Hz");
        syn.updateTempo(140.0); // 140 BPM → 1/4 = 140/60 ≈ 2.333 Hz
        check(std::fabs(syn.vibratoRate() - 140.0f / 60.0f) < 1e-3f,
              "the synced vibrato tracks a tempo change");
        check(std::fabs(syn.vibratoDepth() - 30.0f) < 1e-6f, "vibrato sync leaves the depth alone");
    }

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

        // Velocity → FM index: a hard note is brighter than a soft one when the amount is up.
        auto velBright = [&](float velocity) {
            audio::SynthInstrument s;
            s.setMode(audio::SynthMode::FM);
            s.setFmRatio(1.0f);
            s.setFmIndex(0.5f);       // low base index
            s.setVelToFmIndex(8.0f);  // velocity adds a lot of index
            s.setVelSensitivity(0.0f); // isolate brightness from loudness (equal levels)
            s.setEnvelope(0.002f, 0.02f, 1.0f, 0.05f);
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
        check(velBright(1.0f) > velBright(0.2f) * 1.3,
              "velocity → FM index makes harder notes brighter");
        audio::SynthInstrument dvf;
        check(dvf.velToFmIndex() == 0.0f, "velocity → FM index defaults to 0");
    }

    // --- Oscillator ring modulation ------------------------------------------
    {
        auto ringBright = [&](float amt) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            // osc2 level 0 (ring is the only osc2 use) but detuned a fifth, so the o1×o2 product is
            // a genuine sum/difference partial (no DC artifact from squaring an identical sine).
            s.setOscillators(700.0f, 0.0f, 0.0f, 0.0f);
            s.setRingMod(amt);
            s.noteOn(57, 1.0f); // A3
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            double h = 0.0, en = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
                en += static_cast<double>(out[i]) * out[i];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        // Ring-modulating two detuned oscillators injects a higher sum-frequency partial → brighter.
        check(ringBright(1.0f) > ringBright(0.0f) * 1.3, "ring mod adds inharmonic/high-frequency content");
        audio::SynthInstrument dr;
        check(dr.ringMod() == 0.0f, "ring mod defaults to 0");
    }

    // --- Filter drive (pre-filter saturation) --------------------------------
    {
        auto driveBright = [&](float amt) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine); // near-zero harmonics until driven
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setFilter(12000.0f, 0.7f, 0.0f); // engaged (cutoff < 19 kHz) but open enough to pass harmonics
            s.setFilterDrive(amt);
            s.noteOn(57, 1.0f); // A3
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            double h = 0.0, en = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
                en += static_cast<double>(out[i]) * out[i];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        // Overdriving a pure sine into the filter (tanh) generates odd harmonics → brighter output.
        check(driveBright(1.0f) > driveBright(0.0f) * 1.3,
              "filter drive adds harmonics (drives the pure sine brighter)");
        audio::SynthInstrument dd;
        check(dd.filterDrive() == 0.0f, "filter drive defaults to 0 (clean)");
        // With the filter bypassed (cutoff open) drive is inert — a clean sine stays a clean sine.
        audio::SynthInstrument bypass;
        bypass.setWaveform(audio::Waveform::Sine);
        bypass.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        bypass.setFilter(20000.0f, 0.7f, 0.0f); // open → filter (and its drive) disengaged
        bypass.setFilterDrive(1.0f);
        bypass.noteOn(57, 1.0f);
        const std::vector<float> clean = render(bypass, sampleRate / 4, sampleRate);
        double h = 0.0, en = 0.0;
        for (size_t i = 1; i < clean.size(); ++i) {
            const double d = static_cast<double>(clean[i] - clean[i - 1]);
            h += d * d;
            en += static_cast<double>(clean[i]) * clean[i];
        }
        const double bypassBright = en > 0.0 ? h / en : 0.0;
        check(bypassBright < driveBright(1.0f),
              "filter drive is inert when the filter is bypassed (cutoff open)");
    }

    // --- Velocity → amplitude sensitivity ------------------------------------
    {
        auto renderVel = [&](float velSens, float vel) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setVelSensitivity(velSens);
            s.noteOn(60, vel);
            return rms(render(s, sampleRate / 4, sampleRate));
        };
        // Full sensitivity: a soft (0.3) note is much quieter than a hard (1.0) one.
        check(renderVel(1.0f, 0.3f) < renderVel(1.0f, 1.0f) * 0.5,
              "full velocity sensitivity makes soft notes quieter");
        // Zero sensitivity: loudness is independent of velocity.
        const double quiet = renderVel(0.0f, 0.3f);
        const double loud = renderVel(0.0f, 1.0f);
        check(std::fabs(quiet - loud) < loud * 0.05,
              "zero velocity sensitivity makes loudness ignore velocity");
        audio::SynthInstrument dv;
        check(dv.velSensitivity() == 1.0f, "velocity sensitivity defaults to 1 (full)");
    }

    // --- Per-instrument octave shift -----------------------------------------
    {
        auto crossings = [&](int octave) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOctave(octave);
            s.noteOn(57, 1.0f); // A3 = 220 Hz
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            int c = 0;
            for (size_t i = 1; i < out.size(); ++i) {
                if (out[i - 1] <= 0.0f && out[i] > 0.0f) ++c;
            }
            return c;
        };
        const int base = crossings(0);
        const int up = crossings(1);
        check(base > 0, "synth sounds at the played octave");
        check(up > base * 1.7, "an octave-up shift roughly doubles the pitch");
        audio::SynthInstrument doct;
        check(doct.octave() == 0, "octave shift defaults to 0");
    }

    // --- Monophonic mode -----------------------------------------------------
    {
        audio::SynthInstrument polySyn;
        polySyn.setEnvelope(0.001f, 0.01f, 1.0f, 0.5f);
        polySyn.noteOn(60, 1.0f);
        polySyn.noteOn(64, 1.0f);
        check(polySyn.activeVoices() == 2, "poly mode holds two overlapping notes");

        audio::SynthInstrument monoSyn;
        monoSyn.setMono(true);
        monoSyn.setEnvelope(0.001f, 0.01f, 1.0f, 0.5f);
        monoSyn.noteOn(60, 1.0f);
        monoSyn.noteOn(64, 1.0f); // steals the single voice
        check(monoSyn.activeVoices() == 1, "mono mode holds a single voice");
        audio::SynthInstrument dm;
        check(!dm.mono(), "mono mode defaults off (polyphonic)");
    }

    // --- Wavetable scan LFO --------------------------------------------------
    {
        // Default frames run dark→bright (Sine→…→Square). With the LFO scanning the position, a
        // window at the LFO peak (bright frame) has more HF than one at the trough (dark frame).
        auto windowHf = [&](float depth, int startFrame) {
            audio::SynthInstrument s;
            s.setMode(audio::SynthMode::Wavetable);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setWavetablePosition(0.0f);
            s.setWavetableMorph(0.0f);        // isolate the LFO from the envelope morph
            s.setWavetableLfo(1.0f, depth);   // 1 Hz sweep
            s.noteOn(57, 1.0f);
            const std::vector<float> out = render(s, sampleRate, sampleRate); // 1 s
            double h = 0.0;
            for (int i = startFrame + 1; i < startFrame + 4000; ++i) {
                const double d = static_cast<double>(out[static_cast<size_t>(i)] -
                                                     out[static_cast<size_t>(i - 1)]);
                h += d * d;
            }
            return h;
        };
        // At 1 Hz: t≈0.25 s is the LFO peak (position→1, brightest); t≈0.75 s is the trough (darkest).
        check(windowHf(1.0f, sampleRate / 4) > windowHf(1.0f, 3 * sampleRate / 4) * 1.3,
              "wavetable LFO sweeps the position (brighter at the LFO peak)");
        // With no LFO depth the timbre is static across the two windows.
        const double a = windowHf(0.0f, sampleRate / 4);
        const double b = windowHf(0.0f, 3 * sampleRate / 4);
        check(std::fabs(a - b) < a * 0.2 + 1e-9, "with no LFO the wavetable timbre is static");
        audio::SynthInstrument dl;
        check(dl.wavetableLfoDepth() == 0.0f, "wavetable LFO depth defaults to 0");

        // Velocity → wavetable position: a hard note scans further (brighter) than a soft one.
        auto velWtBright = [&](float velocity) {
            audio::SynthInstrument s;
            s.setMode(audio::SynthMode::Wavetable);
            s.setWavetablePosition(0.0f); // base at the darkest frame
            s.setWavetableMorph(0.0f);
            s.setVelToWavePosition(1.0f); // velocity scans the whole table
            s.setVelSensitivity(0.0f);    // equal levels → isolate brightness
            s.setEnvelope(0.002f, 0.02f, 1.0f, 0.05f);
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
        check(velWtBright(1.0f) > velWtBright(0.1f) * 1.3,
              "velocity → wavetable position makes harder notes scan brighter");
        audio::SynthInstrument dvw;
        check(dvw.velToWavePosition() == 0.0f, "velocity → wavetable position defaults to 0");
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
        check(dsub.subOctave() == 1, "sub octave defaults to 1");

        // Sub octave: a two-octave-down sub makes the signal repeat at a longer period than a
        // one-octave-down sub. Find the fundamental period via autocorrelation.
        auto subPeriod = [&](int octave) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 0.0f, 0.9f, 0.0f); // strong sub (plus the sine carrier)
            s.setSubOctave(octave);
            s.noteOn(69, 1.0f); // A4 = 440 Hz → sub 220 (oct1) or 110 (oct2)
            const std::vector<float> b = render(s, sampleRate / 2, sampleRate);
            int bestLag = 0;
            double best = -1.0;
            for (int lag = 150; lag < 700; ++lag) { // skip the 440 Hz carrier period (~109)
                double acc = 0.0;
                for (size_t i = 0; i + static_cast<size_t>(lag) < b.size(); ++i) {
                    acc += static_cast<double>(b[i]) * static_cast<double>(b[i + static_cast<size_t>(lag)]);
                }
                if (acc > best) {
                    best = acc;
                    bestLag = lag;
                }
            }
            return bestLag;
        };
        check(subPeriod(2) > subPeriod(1) * 1.5,
              "a two-octave-down sub repeats at a longer period than a one-octave sub");

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

        // Osc2 independent waveform: unlinking osc2 to a different shape changes the tone.
        auto o2Render = [&](bool square) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setOscillators(0.0f, 0.8f, 0.0f, 0.0f); // osc2 at the same pitch
            if (square) {
                s.setOsc2Waveform(audio::Waveform::Square);
            }
            s.noteOn(57, 1.0f);
            return render(s, sampleRate / 4, sampleRate);
        };
        const std::vector<float> o2Linked = o2Render(false); // osc2 follows the primary (Saw)
        const std::vector<float> o2Square = o2Render(true);   // osc2 = Square (unlinked)
        double o2Diff = 0.0;
        for (size_t i = 0; i < o2Linked.size(); ++i) {
            o2Diff += std::fabs(static_cast<double>(o2Linked[i] - o2Square[i]));
        }
        check(o2Diff > 1.0, "osc2 can use a different waveform than the primary (changes the tone)");
        audio::SynthInstrument do2;
        check(do2.osc2WaveformLinked() && do2.osc2Waveform() == audio::Waveform::Saw,
              "osc2 waveform defaults to linked (follows the primary)");

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

        // PWM LFO: sweeping the duty cycle makes the pulse's DC/mean wander over time (a 0.5 square
        // has ~0 mean; a narrow pulse a large one). With the LFO off the mean is steady.
        auto meanSpread = [&](float depth) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Square);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
            s.setPulseWidth(0.5f);
            s.setPwmLfo(3.0f, depth);
            s.noteOn(57, 1.0f);
            const std::vector<float> b = render(s, sampleRate, sampleRate); // 1 s
            double lo = 1e18, hi = -1e18;
            const int win = 2000;
            for (int w = 0; w + win <= sampleRate; w += win) {
                double sum = 0.0;
                for (int i = w; i < w + win; ++i) {
                    sum += static_cast<double>(b[static_cast<size_t>(i)]);
                }
                const double m = sum / win;
                lo = std::min(lo, m);
                hi = std::max(hi, m);
            }
            return hi - lo;
        };
        check(meanSpread(0.0f) < 0.02, "with the PWM LFO off the square's duty (mean) is steady");
        check(meanSpread(0.4f) > 0.1, "the PWM LFO sweeps the duty cycle over time");
        audio::SynthInstrument dpwm;
        check(dpwm.pwmLfoDepth() == 0.0f, "PWM LFO depth defaults to 0 (off)");
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

        // Filter key tracking: for a high note, tracking raises the cutoff → brighter than no
        // tracking (measured on the same note, so only the tracking differs).
        auto keyBright = [&](float track) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setFilter(600.0f, 0.7f, 0.0f); // low fixed cutoff
            s.setFilterKeyTrack(track);
            s.noteOn(84, 1.0f); // C6, two octaves above middle C
            const std::vector<float> out = render(s, sampleRate / 4, sampleRate);
            double h = 0.0, en = 0.0;
            for (size_t i = 1; i < out.size(); ++i) {
                const double d = static_cast<double>(out[i] - out[i - 1]);
                h += d * d;
                en += static_cast<double>(out[i]) * out[i];
            }
            return en > 0.0 ? h / en : 0.0;
        };
        check(keyBright(1.0f) > keyBright(0.0f) * 1.3,
              "filter key tracking opens the cutoff for high notes");
        audio::SynthInstrument dkt;
        check(dkt.filterKeyTrack() == 0.0f, "filter key tracking defaults to 0");
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

        // Vibrato onset delay: no pitch modulation before the delay elapses, then it fades in.
        auto renderVib = [&](float depth, float delay) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setVibrato(6.0f, depth);
            s.setVibratoDelay(delay);
            s.noteOn(60, 1.0f);
            return render(s, sampleRate / 4, sampleRate); // 0.25 s
        };
        const std::vector<float> ref = renderVib(0.0f, 0.0f);        // no vibrato at all
        const std::vector<float> delayed = renderVib(80.0f, 0.1f);   // deep vibrato, 100 ms onset
        auto rangeDiff = [](const std::vector<float>& a, const std::vector<float>& b, int lo, int hi) {
            double d = 0.0;
            for (int i = lo; i < hi; ++i) {
                d += std::fabs(static_cast<double>(a[static_cast<size_t>(i)] - b[static_cast<size_t>(i)]));
            }
            return d;
        };
        // Before the delay (first ~60 ms) the delayed-vibrato render matches the no-vibrato render.
        check(rangeDiff(delayed, ref, 0, 3000) < 1e-4,
              "vibrato onset delay: no modulation before the delay elapses");
        // Well after the delay + fade (~190–250 ms) the vibrato is active, so it diverges from steady.
        check(rangeDiff(delayed, ref, 9000, 12000) > 0.1,
              "vibrato fades in after the onset delay");
        audio::SynthInstrument dvd;
        check(dvd.vibratoDelay() == 0.0f, "vibrato delay defaults to 0 (immediate)");
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

        // Legato-only glide: an isolated note (the previous one already released) does NOT glide, so
        // it starts on-pitch; in always mode the same note glides up from the low pitch.
        auto secondNoteEarlyHz = [&](bool legato) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
            s.setGlide(0.20f);
            s.setGlideLegato(legato);
            s.noteOn(57, 1.0f); // establishes lastFreq (~220 Hz)
            (void)render(s, sampleRate / 20, sampleRate);
            s.noteOff(57);
            (void)render(s, sampleRate / 20, sampleRate); // released before the next note
            s.noteOn(81, 1.0f);                            // isolated high note (~880 Hz)
            return estimateHz(render(s, sampleRate / 100, sampleRate), sampleRate); // first 10 ms
        };
        check(secondNoteEarlyHz(false) < 700.0, "always-glide starts the isolated note low");
        check(secondNoteEarlyHz(true) > 800.0,
              "legato-only glide leaves an isolated note on-pitch (no glide)");
        audio::SynthInstrument dg;
        check(!dg.glideLegato(), "glide legato mode defaults off (always glide)");
    }

    // --- Filter cutoff LFO ---------------------------------------------------
    {
        // A sustained bright note through a resonant low-pass. With the cutoff LFO on, the filter
        // sweeps open/closed, so the output loudness swings over the note; with it off, a sustained
        // note holds a near-constant level. Measure the per-window RMS spread.
        auto windowSpread = [&](audio::SynthInstrument& s) {
            s.noteOn(69, 1.0f); // A4
            const std::vector<float> buf = render(s, sampleRate / 2, sampleRate); // 0.5 s sustained
            const int win = 2000;
            double lo = 1e9, hi = 0.0;
            for (size_t start = 0; start + win <= buf.size(); start += win) {
                double sum = 0.0;
                for (int i = 0; i < win; ++i) {
                    const double x = buf[start + static_cast<size_t>(i)];
                    sum += x * x;
                }
                const double r = std::sqrt(sum / win);
                lo = std::min(lo, r);
                hi = std::max(hi, r);
            }
            return hi / (lo + 1e-9); // max/min window-RMS ratio
        };

        audio::SynthInstrument wob;
        wob.setWaveform(audio::Waveform::Saw);
        wob.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        wob.setFilter(500.0f, 6.0f, 0.0f); // resonant LP, no envelope sweep
        wob.setFilterLfo(6.0f, 2.0f);      // 6 Hz, ±2 octaves — the wobble
        const double wobbleRatio = windowSpread(wob);

        audio::SynthInstrument steady;
        steady.setWaveform(audio::Waveform::Saw);
        steady.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        steady.setFilter(500.0f, 6.0f, 0.0f); // same filter, LFO off
        const double steadyRatio = windowSpread(steady);

        check(wobbleRatio > 2.0, "cutoff LFO makes the level swing across the note");
        check(steadyRatio < 1.5, "without the cutoff LFO a sustained note holds steady");
        check(wobbleRatio > steadyRatio * 1.5, "cutoff LFO adds clear movement vs no LFO");

        audio::SynthInstrument df;
        check(df.filterLfoDepth() == 0.0f, "filter cutoff LFO defaults to 0 (off)");
    }

    // --- Amplitude LFO (tremolo) ---------------------------------------------
    {
        // A sustained note with the amp LFO on has its level swing over the note; off, it holds.
        auto windowSpread = [&](audio::SynthInstrument& s) {
            s.noteOn(69, 1.0f);
            const std::vector<float> buf = render(s, sampleRate / 2, sampleRate); // 0.5 s
            const int win = 2000;
            double lo = 1e9, hi = 0.0;
            for (size_t start = 0; start + win <= buf.size(); start += win) {
                double sum = 0.0;
                for (int i = 0; i < win; ++i) {
                    const double x = buf[start + static_cast<size_t>(i)];
                    sum += x * x;
                }
                const double r = std::sqrt(sum / win);
                lo = std::min(lo, r);
                hi = std::max(hi, r);
            }
            return hi / (lo + 1e-9);
        };

        audio::SynthInstrument trem;
        trem.setWaveform(audio::Waveform::Saw);
        trem.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        trem.setAmpLfo(6.0f, 0.9f); // strong 6 Hz tremolo
        const double tremRatio = windowSpread(trem);

        audio::SynthInstrument steady;
        steady.setWaveform(audio::Waveform::Saw);
        steady.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        const double steadyRatio = windowSpread(steady);

        check(tremRatio > 2.0, "amp LFO makes the level swing across the note");
        check(steadyRatio < 1.5, "without the amp LFO a sustained note holds steady");
        check(tremRatio > steadyRatio * 1.5, "amp LFO adds clear level movement vs no LFO");

        audio::SynthInstrument da;
        check(da.ampLfoDepth() == 0.0f, "amp LFO (tremolo) defaults to 0 (off)");
    }

    // --- Analog drift --------------------------------------------------------
    {
        auto renderNote = [&](audio::SynthInstrument& s) {
            s.noteOn(69, 1.0f);
            const std::vector<float> b = render(s, 4000, sampleRate);
            s.noteOff(69);
            (void)render(s, 2000, sampleRate); // let the release finish
            return b;
        };
        auto makeSyn = [&](float driftCents) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
            s.setMono(true); // force the same voice so any difference is the drift itself
            s.setDrift(driftCents);
            return s;
        };

        // Drift off → two identical notes render bit-identically.
        audio::SynthInstrument off = makeSyn(0.0f);
        const std::vector<float> a1 = renderNote(off);
        const std::vector<float> a2 = renderNote(off);
        check(a1 == a2, "with drift off, repeated identical notes are bit-identical");

        // Drift on → each note is detuned by a different random amount, so they differ...
        audio::SynthInstrument on = makeSyn(40.0f);
        const std::vector<float> b1 = renderNote(on);
        const std::vector<float> b2 = renderNote(on);
        check(b1 != b2, "analog drift detunes each note differently (repeats differ)");
        check(std::fabs(estimateHz(b1, sampleRate) - 440.0) < 30.0,
              "a drifted note still sits close to its true pitch");

        // ...but it is deterministic: a fresh synth with the same seed reproduces the sequence.
        audio::SynthInstrument on2 = makeSyn(40.0f);
        const std::vector<float> c1 = renderNote(on2);
        check(b1 == c1, "analog drift is deterministic (same seed → same result)");

        audio::SynthInstrument dd;
        check(dd.drift() == 0.0f, "analog drift defaults to 0 (in tune)");
    }

    // --- Oscillator start-phase randomization --------------------------------
    {
        auto renderNote = [&](audio::SynthInstrument& s) {
            s.noteOn(69, 1.0f);
            const std::vector<float> b = render(s, 4000, sampleRate);
            s.noteOff(69);
            (void)render(s, 2000, sampleRate); // let the release finish before the next note
            return b;
        };
        auto makeSyn = [&](float phaseRand) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw); // phase-dependent shape
            s.setEnvelope(0.0005f, 0.01f, 1.0f, 0.02f);
            s.setMono(true);
            s.setStartPhaseRandom(phaseRand);
            return s;
        };

        // Off → every note starts at phase 0, so repeats are bit-identical.
        audio::SynthInstrument off = makeSyn(0.0f);
        const std::vector<float> a1 = renderNote(off);
        const std::vector<float> a2 = renderNote(off);
        check(a1 == a2, "with phase-random off, repeated notes start bit-identically");

        // On → each note starts at a different random phase, so repeats differ...
        audio::SynthInstrument on = makeSyn(1.0f);
        const std::vector<float> b1 = renderNote(on);
        const std::vector<float> b2 = renderNote(on);
        check(b1 != b2, "start-phase randomization makes repeated notes differ");
        // ...but the pitch is unchanged (phase only shifts where the cycle begins).
        check(std::fabs(estimateHz(b1, sampleRate) - 440.0) < 15.0,
              "start-phase randomization keeps the note's pitch");

        // ...and it is deterministic: same seed reproduces the sequence.
        audio::SynthInstrument on2 = makeSyn(1.0f);
        const std::vector<float> c1 = renderNote(on2);
        check(b1 == c1, "start-phase randomization is deterministic (same seed → same result)");

        audio::SynthInstrument dd;
        check(dd.startPhaseRandom() == 0.0f, "start-phase randomization defaults to 0 (off)");
    }

    // --- Per-note fine tune --------------------------------------------------
    {
        audio::SynthInstrument s;
        s.setWaveform(audio::Waveform::Saw);
        s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
        s.noteOn(69, 1.0f, 0.0f); // A4 = 440 Hz, no fine tune
        const std::vector<float> a = render(s, 8000, sampleRate);
        s.allNotesOff();
        (void)render(s, 2000, sampleRate);
        s.noteOn(69, 1.0f, 200.0f); // +200 cents = +2 semitones → ~493.9 Hz
        const std::vector<float> b = render(s, 8000, sampleRate);
        const double ha = estimateHz(a, sampleRate);
        const double hb = estimateHz(b, sampleRate);
        check(std::fabs(ha - 440.0) < 12.0, "a note with no fine tune plays at its pitch");
        check(hb > ha * 1.10, "a +200-cent fine tune raises the note ~2 semitones");

        // PianoRoll per-note accessor.
        audio::PianoRoll fr;
        fr.addNote(audio::Note{0, 4, 60, 1.0f});
        check(std::fabs(fr.setNoteFineTune(60, 0, 50.0f) - 50.0f) < 1e-4f, "note fine tune is settable");
        check(std::fabs(fr.noteFineTune(60, 0) - 50.0f) < 1e-4f, "note fine tune reads back");
        check(std::fabs(fr.setNoteFineTune(60, 0, 999.0f) - 200.0f) < 1e-4f,
              "note fine tune clamps to +/-200 cents");
        check(fr.noteFineTune(62, 0) == 0.0f, "an empty cell reports 0 fine tune");
    }

    // --- Third oscillator ----------------------------------------------------
    {
        // A pure sine plus a third oscillator an octave up adds high-frequency content: the
        // first-difference ("HF") energy relative to total energy rises when osc3 is on.
        auto hfRatio = [&](float osc3lvl) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
            s.setOsc3Level(osc3lvl);
            s.setOsc3Semitones(12.0f); // an octave up
            s.noteOn(69, 1.0f);
            const std::vector<float> b = render(s, 8000, sampleRate);
            double hf = 0.0, en = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double d = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                hf += d * d;
                en += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        const double off = hfRatio(0.0f);
        const double on = hfRatio(1.0f);
        check(off > 0.0, "a single sine osc has a baseline HF ratio");
        check(on > off * 1.5, "a third oscillator an octave up adds high-frequency content");

        audio::SynthInstrument d;
        check(d.osc3Level() == 0.0f, "osc3 defaults to off");
    }

    // --- Filter type (LP / HP / BP) ------------------------------------------
    {
        // A saw through the filter at a mid cutoff: low-pass keeps the lows (low HF ratio), high-pass
        // removes them (much higher HF ratio).
        auto hfRatio = [&](audio::StateVariableFilter::Mode m) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Saw);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
            s.setFilter(600.0f, 1.0f, 0.0f);
            s.setFilterMode(m);
            s.noteOn(57, 1.0f); // A3 = 220 Hz
            const std::vector<float> b = render(s, 8000, sampleRate);
            double hf = 0.0, en = 0.0;
            for (size_t i = 1; i < b.size(); ++i) {
                const double dd = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                hf += dd * dd;
                en += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        const double lp = hfRatio(audio::StateVariableFilter::Mode::LowPass);
        const double hp = hfRatio(audio::StateVariableFilter::Mode::HighPass);
        check(lp > 0.0, "low-pass filter produces sound");
        check(hp > lp * 2.0, "high-pass keeps highs and removes lows (higher HF ratio than low-pass)");

        // Notch: a pure tone sitting at the cutoff is rejected, whereas low-pass passes it.
        auto rmsAtCutoff = [&](audio::StateVariableFilter::Mode m) {
            audio::SynthInstrument s;
            s.setWaveform(audio::Waveform::Sine);
            s.setEnvelope(0.001f, 0.01f, 1.0f, 0.02f);
            s.setFilter(220.0f, 1.0f, 0.0f); // cutoff on the note's fundamental
            s.setFilterMode(m);
            s.noteOn(57, 1.0f); // A3 = 220 Hz
            return rms(render(s, 8000, sampleRate));
        };
        check(rmsAtCutoff(audio::StateVariableFilter::Mode::Notch) <
                  rmsAtCutoff(audio::StateVariableFilter::Mode::LowPass) * 0.5,
              "notch rejects a tone at the cutoff (much quieter than low-pass)");

        audio::SynthInstrument df;
        check(df.filterMode() == audio::StateVariableFilter::Mode::LowPass,
              "filter type defaults to low-pass");
    }

    // --- Dedicated filter envelope -------------------------------------------
    {
        // A sustained note with a low base cutoff and a filter envelope that sweeps the cutoff up and
        // decays back: the start is bright (open filter), the tail is dark (closed) — independent of
        // the (sustained) amp envelope.
        auto hfWindow = [&](const std::vector<float>& b, int a, int c) {
            double hf = 0.0, en = 0.0;
            for (int i = a + 1; i < c; ++i) {
                const double d = static_cast<double>(b[i]) - static_cast<double>(b[i - 1]);
                hf += d * d;
                en += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return en > 0.0 ? hf / en : 0.0;
        };
        audio::SynthInstrument s;
        s.setWaveform(audio::Waveform::Saw);
        s.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f); // sustained amplitude
        s.setFilter(400.0f, 1.0f, 0.0f);           // low base cutoff, no amp-env cutoff
        s.setFilterEnvelope(0.001f, 0.05f, 0.0f, 0.05f); // fast attack, 50 ms decay to 0 sustain
        s.setFilterEnvDepth(8000.0f);                    // sweep +8 kHz, then back to base
        s.noteOn(57, 1.0f);
        const std::vector<float> buf = render(s, sampleRate / 2, sampleRate);
        const double early = hfWindow(buf, 200, 1400);    // during the sweep (bright)
        const double late = hfWindow(buf, 12000, 24000);  // long after the decay (dark)
        check(early > late * 1.5, "filter envelope opens the cutoff early then closes it (brighter start)");

        // With depth 0 (default) the cutoff is steady, so early and late are similar.
        audio::SynthInstrument flat;
        flat.setWaveform(audio::Waveform::Saw);
        flat.setEnvelope(0.001f, 0.01f, 1.0f, 0.05f);
        flat.setFilter(400.0f, 1.0f, 0.0f);
        flat.noteOn(57, 1.0f);
        const std::vector<float> fb = render(flat, sampleRate / 2, sampleRate);
        const double fe = hfWindow(fb, 200, 1400);
        const double fl = hfWindow(fb, 12000, 24000);
        check(fe < fl * 1.5 && fl < fe * 1.5, "no filter envelope → a steady cutoff (no sweep)");

        audio::SynthInstrument d;
        check(d.filterEnvDepth() == 0.0f, "filter envelope depth defaults to 0 (off)");
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

    // Partial quantize: strength moves notes only part of the way to the grid.
    {
        audio::PianoRoll pq;
        pq.addNote(audio::Note{2, 1, 60, 1.0f}); // nearest multiple of 8 is 0; halfway → 1
        pq.quantizeStrength(8, 0.5f);
        check(pq.notes()[0].startStep == 1, "half-strength quantize moves a note halfway to the grid");

        audio::PianoRoll pf;
        pf.addNote(audio::Note{2, 1, 60, 1.0f});
        pf.quantizeStrength(8, 1.0f);
        check(pf.notes()[0].startStep == 0, "full-strength quantize snaps all the way");

        audio::PianoRoll pz;
        pz.addNote(audio::Note{2, 1, 60, 1.0f});
        check(pz.quantizeStrength(8, 0.0f) == 0 && pz.notes()[0].startStep == 2,
              "zero-strength quantize leaves notes put");
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

        // Extended voicings: a major 9th is five notes and includes the 9th (D an octave up).
        audio::PianoRoll m9;
        check(m9.addChord(0, 1, 60, audio::Chord::Maj9) == 5, "a major 9th is five notes");
        check(m9.hasNote(74, 0), "major 9th adds the ninth (D above the octave)");
        check(m9.hasNote(71, 0), "major 9th keeps the major seventh");
        // A 6th chord adds the major sixth.
        audio::PianoRoll six;
        check(six.addChord(0, 1, 60, audio::Chord::Maj6) == 4 && six.hasNote(69, 0),
              "a 6th chord adds the major sixth (A above C)");
        // Add9 is a triad plus the ninth, no seventh.
        audio::PianoRoll a9;
        a9.addChord(0, 1, 60, audio::Chord::Add9);
        check(a9.hasNote(74, 0) && !a9.hasNote(70, 0) && !a9.hasNote(71, 0),
              "add9 has the ninth but no seventh");

        // Diminished 7th: root + m3 + b5 + bb7 (0,3,6,9).
        audio::PianoRoll dim7;
        check(dim7.addChord(0, 1, 60, audio::Chord::Dim7) == 4, "a diminished 7th is four notes");
        check(dim7.hasNote(60, 0) && dim7.hasNote(63, 0) && dim7.hasNote(66, 0) && dim7.hasNote(69, 0),
              "dim7 stacks minor thirds (0,3,6,9)");

        // Half-diminished (m7b5): root + m3 + b5 + b7 (0,3,6,10).
        audio::PianoRoll hd;
        check(hd.addChord(0, 1, 60, audio::Chord::HalfDim7) == 4, "a half-diminished 7th is four notes");
        check(hd.hasNote(60, 0) && hd.hasNote(63, 0) && hd.hasNote(66, 0) && hd.hasNote(70, 0),
              "m7b5 is a diminished triad with a minor 7th (0,3,6,10)");
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

    // --- Randomize (humanize) timing -----------------------------------------
    {
        auto build = [](audio::PianoRoll& p) {
            for (int i = 0; i < 8; ++i) {
                p.addNote(audio::Note{4 + i, 1, 60, 0.9f}); // starts 4..11, away from 0
            }
        };
        audio::PianoRoll a;
        audio::PianoRoll b;
        build(a);
        build(b);
        a.randomizeTiming(2, 777u);
        b.randomizeTiming(2, 777u); // same seed → identical

        bool deterministic = true, inRange = true, anyMoved = false;
        for (size_t i = 0; i < a.notes().size(); ++i) {
            const int start = a.notes()[i].startStep;
            if (start != b.notes()[i].startStep) deterministic = false;
            const int orig = 4 + static_cast<int>(i);
            if (start < orig - 2 || start > orig + 2) inRange = false; // within ±2 (none clamp here)
            if (start != orig) anyMoved = true;
        }
        check(deterministic, "timing randomize is deterministic for a given seed");
        check(inRange, "timing randomize stays within the requested range");
        check(anyMoved, "timing randomize actually nudges note starts");

        // Clamp at 0: a note near the start never goes negative.
        audio::PianoRoll c;
        c.addNote(audio::Note{0, 1, 60, 0.9f});
        c.randomizeTiming(3, 5u);
        check(c.notes()[0].startStep >= 0, "timing randomize never produces a negative start");
    }

    // --- Duplicate -----------------------------------------------------------
    {
        audio::PianoRoll dr;
        dr.addNote(audio::Note{0, 1, 60, 1.0f});
        dr.addNote(audio::Note{2, 1, 64, 0.7f});
        const int added = dr.duplicate(4);
        check(added == 2 && dr.notes().size() == 4, "duplicate appends a copy of every note");
        check(dr.hasNote(60, 4) && dr.hasNote(64, 6),
              "the duplicated notes are shifted later by the offset");
        // The copy keeps pitch/velocity; a zero/negative offset is a no-op.
        check(dr.duplicate(0) == 0 && dr.notes().size() == 4, "a zero-offset duplicate is a no-op");
    }

    // --- Velocity ramp -------------------------------------------------------
    {
        // Four notes across the bar; a 0.2 → 1.0 ramp should swell linearly by start position.
        audio::PianoRoll vr;
        vr.addNote(audio::Note{0, 1, 60, 0.5f});
        vr.addNote(audio::Note{4, 1, 62, 0.5f});
        vr.addNote(audio::Note{8, 1, 64, 0.5f});
        vr.addNote(audio::Note{12, 1, 65, 0.5f});
        const int changed = vr.velocityRamp(0.2f, 1.0f);
        check(changed == 4, "velocity ramp adjusts every note");
        auto velAt = [&](int start) {
            for (const auto& nt : vr.notes())
                if (nt.startStep == start) return nt.velocity;
            return -1.0f;
        };
        check(std::fabs(velAt(0) - 0.2f) < 1e-3f, "ramp sets the first note to the start velocity");
        check(std::fabs(velAt(12) - 1.0f) < 1e-3f, "ramp sets the last note to the end velocity");
        // Interpolated by start position over the 0..12 span: 4/12 and 8/12 of the way from 0.2→1.0.
        check(std::fabs(velAt(4) - (0.2f + 0.8f * 4.0f / 12.0f)) < 1e-3f &&
                  std::fabs(velAt(8) - (0.2f + 0.8f * 8.0f / 12.0f)) < 1e-3f,
              "in-between notes interpolate linearly by start position");
        check(velAt(0) < velAt(4) && velAt(4) < velAt(8) && velAt(8) < velAt(12),
              "the ramp is monotonically increasing");

        // A descending ramp fades out; notes sharing a start step all take the start velocity.
        audio::PianoRoll fr;
        fr.addNote(audio::Note{0, 1, 60, 0.5f});
        fr.addNote(audio::Note{0, 1, 64, 0.5f});
        fr.addNote(audio::Note{8, 1, 67, 0.5f});
        fr.velocityRamp(1.0f, 0.0f);
        int downCount = 0;
        for (const auto& nt : fr.notes())
            if (nt.startStep == 0) { check(std::fabs(nt.velocity - 1.0f) < 1e-3f, "same-start notes share the ramp velocity"); ++downCount; }
        check(downCount == 2, "both notes on the first step were ramped");
        check(vr.velocityRamp(0.5f, 0.5f) >= 0, "a flat ramp is well-defined");
    }

    // --- Chop ----------------------------------------------------------------
    {
        // An 8-step note chopped into 4 becomes four 2-step notes at 0,2,4,6, same pitch/velocity.
        audio::PianoRoll cr;
        cr.addNote(audio::Note{0, 8, 60, 0.8f});
        const int chopped = cr.chop(4);
        check(chopped == 1 && cr.notes().size() == 4, "chop splits a note into the requested pieces");
        auto& ns = cr.notes();
        bool spacing = true, sameData = true;
        for (int p = 0; p < 4; ++p) {
            if (ns[static_cast<size_t>(p)].startStep != p * 2 ||
                ns[static_cast<size_t>(p)].lengthSteps != 2) {
                spacing = false;
            }
            if (ns[static_cast<size_t>(p)].pitch != 60 ||
                std::fabs(ns[static_cast<size_t>(p)].velocity - 0.8f) > 1e-6f) {
                sameData = false;
            }
        }
        check(spacing, "chopped pieces are evenly spaced and equal length");
        check(sameData, "chopped pieces keep the source pitch and velocity");

        // A note too short to split into whole-step pieces is left untouched; pieces < 2 is a no-op.
        audio::PianoRoll sr2;
        sr2.addNote(audio::Note{0, 3, 62, 1.0f});
        check(sr2.chop(4) == 0 && sr2.notes().size() == 1, "a note shorter than pieces is not chopped");
        audio::PianoRoll np;
        np.addNote(audio::Note{0, 8, 64, 1.0f});
        check(np.chop(1) == 0 && np.notes().size() == 1, "chop with <2 pieces is a no-op");
    }

    // --- Arpeggiate (bake a chord into notes) --------------------------------
    {
        // A C-E-G triad (60/64/67) lasting 8 steps, arpeggiated up at length 2 → 60,64,67,60 at
        // steps 0,2,4,6.
        audio::PianoRoll ar;
        ar.addNote(audio::Note{0, 8, 64, 0.9f});
        ar.addNote(audio::Note{0, 8, 60, 0.9f});
        ar.addNote(audio::Note{0, 8, 67, 0.9f});
        const int made = ar.arpeggiate(2, 0);
        check(made == 4 && ar.notes().size() == 4, "arpeggiate prints one note per sub-step");
        auto pitchAt = [&](int start) {
            for (const auto& nt : ar.notes())
                if (nt.startStep == start) return nt.pitch;
            return -1;
        };
        check(pitchAt(0) == 60 && pitchAt(2) == 64 && pitchAt(4) == 67 && pitchAt(6) == 60,
              "arpeggiate walks the chord pitches upward, cycling");
        bool len2 = true;
        for (const auto& nt : ar.notes())
            if (nt.lengthSteps != 2) len2 = false;
        check(len2, "arpeggiated notes take the requested length");

        // Down mode starts from the top of the chord.
        audio::PianoRoll dn;
        dn.addNote(audio::Note{0, 4, 60, 1.0f});
        dn.addNote(audio::Note{0, 4, 64, 1.0f});
        dn.addNote(audio::Note{0, 4, 67, 1.0f});
        dn.arpeggiate(2, 1);
        check(dn.notes().front().pitch == 67, "arpeggiate-down starts from the highest pitch");

        // A single (non-chord) note and noteLen < 1 are left untouched.
        audio::PianoRoll one;
        one.addNote(audio::Note{0, 8, 60, 1.0f});
        one.arpeggiate(2, 0);
        check(one.notes().size() == 1 && one.notes().front().lengthSteps == 8,
              "a single note is not arpeggiated");
        audio::PianoRoll np2;
        np2.addNote(audio::Note{0, 8, 60, 1.0f});
        np2.addNote(audio::Note{0, 8, 64, 1.0f});
        check(np2.arpeggiate(0, 0) == 0 && np2.notes().size() == 2, "arpeggiate with len<1 is a no-op");
    }

    // --- Transpose -----------------------------------------------------------
    {
        audio::PianoRoll tr;
        tr.addNote(audio::Note{0, 1, 60, 1.0f});
        tr.addNote(audio::Note{2, 1, 64, 1.0f});
        const int changed = tr.transpose(12);
        check(changed == 2, "transpose shifts every note");
        check(tr.notes()[0].pitch == 72 && tr.notes()[1].pitch == 76,
              "transpose moves pitches by the given semitones");
        check(tr.transpose(0) == 0, "a zero transpose is a no-op");

        // Pitches clamp to the MIDI range.
        audio::PianoRoll cl;
        cl.addNote(audio::Note{0, 1, 5, 1.0f});
        cl.transpose(-100);
        check(cl.notes()[0].pitch == 0, "transpose clamps at the low end of the MIDI range");
    }

    // --- Stretch (time-scale) ------------------------------------------------
    {
        audio::PianoRoll st;
        st.addNote(audio::Note{0, 2, 60, 1.0f});
        st.addNote(audio::Note{4, 2, 64, 1.0f});
        const int changed = st.stretch(2.0f); // double-time (slower/expanded)
        check(changed == 2, "stretch scales every note that moves");
        check(st.notes()[0].startStep == 0 && st.notes()[0].lengthSteps == 4,
              "stretch x2 doubles start and length");
        check(st.notes()[1].startStep == 8 && st.notes()[1].lengthSteps == 4,
              "stretch x2 pushes later notes out proportionally");
        check(st.notes()[0].pitch == 60 && st.notes()[1].pitch == 64,
              "stretch leaves pitches untouched");

        // Compress back down; lengths never fall below 1 step.
        audio::PianoRoll sc;
        sc.addNote(audio::Note{4, 1, 60, 1.0f});
        sc.stretch(0.5f);
        check(sc.notes()[0].startStep == 2 && sc.notes()[0].lengthSteps == 1,
              "stretch x0.5 compresses starts and floors length at 1");
        check(sc.stretch(1.0f) == 0 && sc.stretch(0.0f) == 0, "stretch by 1 or 0 is a no-op");
    }

    // --- Note-length scale (gate) --------------------------------------------
    {
        audio::PianoRoll gl;
        gl.addNote(audio::Note{0, 4, 60, 1.0f});
        gl.addNote(audio::Note{4, 2, 64, 1.0f});
        const int changed = gl.scaleLengths(0.5f); // staccato
        check(changed == 2, "gate scales every note that changes length");
        check(gl.notes()[0].lengthSteps == 2 && gl.notes()[1].lengthSteps == 1,
              "gate halves note lengths (floored at 1) and leaves starts put");
        check(gl.notes()[0].startStep == 0 && gl.notes()[1].startStep == 4,
              "gate does not move note starts");
        // Lengthen back (legato/overlap) and confirm the no-op cases.
        audio::PianoRoll gl2;
        gl2.addNote(audio::Note{0, 2, 60, 1.0f});
        gl2.scaleLengths(3.0f);
        check(gl2.notes()[0].lengthSteps == 6, "gate can lengthen notes past their neighbours");
        check(gl2.scaleLengths(1.0f) == 0 && gl2.scaleLengths(0.0f) == 0,
              "gate by 1 or 0 is a no-op");
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
