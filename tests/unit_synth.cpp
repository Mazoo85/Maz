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
