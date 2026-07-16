// Unit test for .cjc project save/load — a full round-trip of the musical + mixing state through a
// file, with no audio device. Proves loadProject(saveProject(x)) == x for tempo, the drum grid, the
// piano-roll notes, bus gains, and every mixer effect parameter.

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

    const std::string path = "unit_project_roundtrip.cjc";
    std::string err;
    check(audio::saveProject(path, seq, mixer, &err), "saveProject succeeds");

    // Load into fresh, differently-initialised objects.
    audio::Sequencer seq2;
    audio::Mixer mixer2;
    seq2.setBpm(90.0); // will be overwritten
    check(audio::loadProject(path, seq2, mixer2, &err), "loadProject succeeds");

    // Transport + bus.
    check(near(static_cast<float>(seq2.bpm()), 137.0f), "bpm round-trips");
    check(near(seq2.drumGain(), 0.8f) && near(seq2.synthGain(), 1.2f), "bus gains round-trip");

    // Drum grid.
    check(seq2.step(0, 0) && seq2.step(1, 4) && seq2.step(2, 7), "active steps round-trip");
    check(!seq2.step(0, 1) && !seq2.step(3, 0), "inactive steps stay off");

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

    // A non-.cjc file is rejected.
    audio::Sequencer seq3;
    audio::Mixer mixer3;
    check(!audio::loadProject("/nonexistent/definitely_missing.cjc", seq3, mixer3, &err),
          "loading a missing file fails cleanly");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
