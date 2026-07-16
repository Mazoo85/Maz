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

    // --- PianoRoll model -----------------------------------------------------
    audio::PianoRoll roll;
    check(roll.notes().empty(), "roll starts empty");
    roll.toggle(60, 0);
    check(roll.hasNote(60, 0) && roll.notes().size() == 1, "toggle adds a note");
    roll.toggle(60, 0);
    check(!roll.hasNote(60, 0) && roll.notes().empty(), "toggling again removes it");

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

    std::vector<float> firstStep(6000, 0.0f);
    seq.render(firstStep.data(), 6000, sampleRate);
    check(rms(firstStep) > 0.0, "a scheduled note sounds on its start step");

    // An empty roll with no drums stays silent.
    audio::Sequencer quiet;
    quiet.play();
    std::vector<float> nothing(6000, 0.0f);
    quiet.render(nothing.data(), 6000, sampleRate);
    check(rms(nothing) == 0.0, "empty pattern renders silence");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
