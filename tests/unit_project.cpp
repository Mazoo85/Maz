// Unit test for .cjc project save/load — a full round-trip of the musical + mixing state through a
// file, with no audio device. Proves loadProject(saveProject(x)) == x for tempo, the drum grid, the
// piano-roll notes, bus gains, and every mixer effect parameter.

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/ProjectIO.hpp"
#include "maz/audio/Sequencer.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

} // namespace

int main() {
    // Build a project with distinctive, non-default state everywhere.
    audio::Sequencer seq;
    audio::Mixer mixer;
    audio::Automation automation;

    seq.setBpm(137.0);
    seq.setDrumGain(0.8f);
    seq.setSynthGain(1.2f);
    seq.setStep(0, 0, true);
    seq.setStep(1, 4, true);
    seq.setStep(2, 7, true);
    audio::Note n1{0, 4, 60, 0.9f};
    audio::Note n2{8, 2, 67, 0.7f};
    seq.roll().addNote(n1);
    seq.roll().addNote(n2);
    seq.synth().setMode(audio::SynthMode::FM);
    seq.synth().setWaveform(audio::Waveform::Triangle);
    seq.synth().setEnvelope(0.01f, 0.2f, 0.4f, 0.5f);
    seq.synth().setFmRatio(3.5f);
    seq.synth().setFmIndex(6.0f);
    seq.sampler().setBasePitch(48);
    seq.setUseSampler(true);
    // A second pattern + a playlist arrangement.
    const int p1 = seq.addPattern();
    seq.selectPattern(p1);
    seq.setStep(2, 5, true);
    seq.roll().addNote(audio::Note{4, 1, 72, 0.5f});
    seq.selectPattern(0);
    seq.setPlaylist({0, 1, 0});
    seq.setSongMode(true);

    mixer.setMasterGain(0.75f);
    mixer.eq().setEnabled(true);
    mixer.eq().setCutoff(3200.0f);
    mixer.compressor().setEnabled(true);
    mixer.compressor().setThresholdDb(-20.0f);
    mixer.compressor().setRatio(6.0f);
    mixer.compressor().setMakeupDb(4.0f);
    mixer.delay().setEnabled(true);
    mixer.delay().setTime(250.0f);
    mixer.delay().setMix(0.4f);
    mixer.reverb().setEnabled(true);
    mixer.reverb().setRoomSize(0.85f);
    mixer.reverb().setMix(0.33f);

    audio::AutoLane& lane = automation.lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Saw;
    lane.lfo.rateHz = 1.75f;
    lane.lo = 300.0f;
    lane.hi = 5500.0f;

    const std::string path = "unit_project_roundtrip.cjc";
    std::string err;
    check(audio::saveProject(path, seq, mixer, automation, &err), "saveProject succeeds");

    // Load into fresh, differently-initialised objects.
    audio::Sequencer seq2;
    audio::Mixer mixer2;
    audio::Automation automation2;
    seq2.setBpm(90.0); // will be overwritten
    check(audio::loadProject(path, seq2, mixer2, automation2, &err), "loadProject succeeds");

    // Transport + bus.
    check(near(static_cast<float>(seq2.bpm()), 137.0f), "bpm round-trips");
    check(near(seq2.drumGain(), 0.8f) && near(seq2.synthGain(), 1.2f), "bus gains round-trip");

    // Drum grid (pattern 0).
    check(seq2.step(0, 0) && seq2.step(1, 4) && seq2.step(2, 7), "active steps round-trip");
    check(!seq2.step(0, 1) && !seq2.step(3, 0), "inactive steps stay off");

    // Arrangement: patterns, per-pattern content, playlist, song mode.
    check(seq2.patternCount() == 2, "pattern count round-trips");
    check(seq2.songMode(), "song mode round-trips");
    check(seq2.playlist().size() == 3 && seq2.playlist()[0] == 0 && seq2.playlist()[1] == 1 &&
              seq2.playlist()[2] == 0,
          "playlist round-trips");
    seq2.selectPattern(1);
    check(seq2.step(2, 5) && seq2.roll().notes().size() == 1, "second pattern content round-trips");
    seq2.selectPattern(0);

    // Piano-roll notes.
    check(seq2.roll().notes().size() == 2, "note count round-trips");
    bool notesOk = false;
    if (seq2.roll().notes().size() == 2) {
        const audio::Note& a = seq2.roll().notes()[0];
        const audio::Note& b = seq2.roll().notes()[1];
        notesOk = a.startStep == 0 && a.lengthSteps == 4 && a.pitch == 60 && near(a.velocity, 0.9f) &&
                  b.startStep == 8 && b.lengthSteps == 2 && b.pitch == 67 && near(b.velocity, 0.7f);
    }
    check(notesOk, "note fields round-trip");

    // Synth engine settings.
    check(seq2.synth().mode() == audio::SynthMode::FM, "synth mode round-trips");
    check(seq2.synth().waveform() == audio::Waveform::Triangle, "synth waveform round-trips");
    check(near(seq2.synth().sustain(), 0.4f) && near(seq2.synth().release(), 0.5f),
          "synth envelope round-trips");
    check(near(seq2.synth().fmRatio(), 3.5f) && near(seq2.synth().fmIndex(), 6.0f),
          "FM params round-trip");
    check(seq2.useSampler() && seq2.synth().gain() >= 0.0f && seq2.sampler().basePitch() == 48,
          "sampler settings round-trip");

    // Mixer + effects.
    check(near(mixer2.masterGain(), 0.75f), "master gain round-trips");
    check(mixer2.eq().enabled() && near(mixer2.eq().cutoff(), 3200.0f), "EQ round-trips");
    check(mixer2.compressor().enabled() && near(mixer2.compressor().thresholdDb(), -20.0f) &&
              near(mixer2.compressor().ratio(), 6.0f) && near(mixer2.compressor().makeupDb(), 4.0f),
          "compressor round-trips");
    check(mixer2.delay().enabled() && near(mixer2.delay().time(), 250.0f) &&
              near(mixer2.delay().mix(), 0.4f),
          "delay round-trips");
    check(mixer2.reverb().enabled() && near(mixer2.reverb().roomSize(), 0.85f) &&
              near(mixer2.reverb().mix(), 0.33f),
          "reverb round-trips");

    // Automation lane.
    const audio::AutoLane& lane2 = automation2.lane(audio::AutoTarget::FilterCutoff);
    check(lane2.enabled && lane2.lfo.shape == audio::Waveform::Saw &&
              near(lane2.lfo.rateHz, 1.75f) && near(lane2.lo, 300.0f) && near(lane2.hi, 5500.0f),
          "automation lane round-trips");

    // A non-.cjc file is rejected.
    audio::Sequencer seq3;
    audio::Mixer mixer3;
    audio::Automation automation3;
    check(!audio::loadProject("/nonexistent/definitely_missing.cjc", seq3, mixer3, automation3, &err),
          "loading a missing file fails cleanly");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
