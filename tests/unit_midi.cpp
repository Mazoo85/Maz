// Unit test for MIDI export — write a Standard MIDI File for a known pattern and verify the header,
// track chunk, and that note-on events are present. No audio device.

#include "maz/audio/MidiReader.hpp"
#include "maz/audio/MidiWriter.hpp"
#include "maz/audio/Sequencer.hpp"

#include <cstdint>
#include <cstdio>
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

bool tagAt(const std::vector<uint8_t>& b, size_t off, const char* t) {
    return b.size() >= off + 4 && b[off] == static_cast<uint8_t>(t[0]) &&
           b[off + 1] == static_cast<uint8_t>(t[1]) && b[off + 2] == static_cast<uint8_t>(t[2]) &&
           b[off + 3] == static_cast<uint8_t>(t[3]);
}

} // namespace

int main() {
    audio::Sequencer seq;
    seq.setStep(0, 0, true);  // kick
    seq.setStep(1, 4, true);  // snare
    seq.roll().addNote(audio::Note{0, 4, 60, 0.9f});
    seq.roll().addNote(audio::Note{8, 4, 67, 0.8f});

    const std::string path = "unit_midi_out.mid";
    std::string err;
    check(audio::writeMidi(path, seq, 96, &err), "writeMidi succeeds");

    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    check(b.size() > 22, "file has header + track");
    check(tagAt(b, 0, "MThd"), "starts with MThd");
    // Header: format 0, ntracks 1, division 96.
    check(b.size() > 13 && b[8] == 0 && b[9] == 0, "format 0");
    check(b[10] == 0 && b[11] == 1, "one track");
    check(b[12] == 0 && b[13] == 96, "division = 96 ppq");
    check(tagAt(b, 14, "MTrk"), "track chunk follows");

    // At least one note-on (0x90 melody or 0x99 drums) and an end-of-track meta event.
    bool hasNoteOn = false, hasEot = false;
    for (size_t i = 22; i + 2 < b.size(); ++i) {
        if (b[i] == 0x90 || b[i] == 0x99) {
            hasNoteOn = true;
        }
        if (b[i] == 0xFF && b[i + 1] == 0x2F && b[i + 2] == 0x00) {
            hasEot = true;
        }
    }
    check(hasNoteOn, "contains note-on events");
    check(hasEot, "ends with an end-of-track meta event");

    // --- Import round-trip: read the file we just wrote back into a fresh sequencer ---------------
    audio::Sequencer in;
    check(audio::readMidi(path, in, &err), "readMidi succeeds");
    // The two melodic notes come back onto the lead roll with their pitches, timing, and velocity.
    const auto& notes = in.roll().notes();
    check(notes.size() == 2, "both melodic notes import");
    bool gotC = false, gotG = false;
    for (const audio::Note& n : notes) {
        if (n.pitch == 60 && n.startStep == 0 && n.lengthSteps == 4) {
            gotC = true;
        }
        if (n.pitch == 67 && n.startStep == 8 && n.lengthSteps == 4) {
            gotG = true;
        }
    }
    check(gotC && gotG, "imported notes keep pitch, start, and length");
    // The two drum hits come back on the grid.
    check(in.step(0, 0) && in.step(1, 4), "imported drum hits land on the grid");
    check(!in.step(0, 1), "unset drum steps stay off after import");

    // A non-MIDI file fails cleanly.
    audio::Sequencer bad;
    check(!audio::readMidi("/nonexistent/missing.mid", bad, &err), "reading a missing file fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
