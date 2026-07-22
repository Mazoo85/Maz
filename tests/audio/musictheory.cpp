// tests/audio/musictheory.cpp — verifies the note<->pitch helpers (audio::midiToFrequency / frequencyToMidi /
// noteNameToMidi / midiToNoteName / noteNameToFrequency). Ground truths: A4 (MIDI 69) = 440 Hz, middle C (60) ~
// 261.63, +12 doubles the pitch; frequency<->MIDI round-trips; scientific-pitch names parse (letters, sharps,
// flats, negative octaves) and validate to [0,127]; MIDI renders back to a sharp name; malformed names return -1.
// Values checked against the 12-TET/A440 standard. Pure CPU.
#include "maz/audio/MusicTheory.hpp"

#include <cmath>
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::audio;

static bool close(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. MIDI -> frequency reference points. ---
    {
        CHECK(close(midiToFrequency(69), 440.0, 1e-3), "A4 (69) = 440 Hz");
        CHECK(close(midiToFrequency(60), 261.6256, 1e-2), "middle C (60) ~ 261.63 Hz");
        CHECK(close(midiToFrequency(81), 880.0, 1e-3), "A5 (81) = 880 Hz (octave up doubles)");
        CHECK(close(midiToFrequency(57), 220.0, 1e-3), "A3 (57) = 220 Hz (octave down halves)");
    }

    // --- 2. frequency -> MIDI is the inverse. ---
    {
        CHECK(close(frequencyToMidi(440.0), 69.0, 1e-6), "440 Hz -> 69");
        CHECK(close(frequencyToMidi(880.0), 81.0, 1e-6), "880 Hz -> 81");
        // round-trip several notes.
        bool rt = true;
        for (int m = 0; m <= 127; ++m)
            if (std::llround(frequencyToMidi(static_cast<double>(midiToFrequency(m)))) != m) rt = false;
        CHECK(rt, "midi -> freq -> nearest midi round-trips for all 128 notes");
    }

    // --- 3. Note names -> MIDI. ---
    {
        CHECK(noteNameToMidi("A4") == 69, "A4 = 69");
        CHECK(noteNameToMidi("C4") == 60, "C4 = 60 (middle C)");
        CHECK(noteNameToMidi("C#4") == 61 && noteNameToMidi("Db4") == 61, "C#4 == Db4 == 61 (enharmonic)");
        CHECK(noteNameToMidi("C-1") == 0, "C-1 = 0 (lowest MIDI)");
        CHECK(noteNameToMidi("G9") == 127, "G9 = 127 (highest MIDI)");
        CHECK(noteNameToMidi("b3") == 59 && noteNameToMidi("B3") == 59, "case-insensitive letter (B3 = 59)");
        CHECK(noteNameToMidi("Cb4") == 59, "Cb4 = 59 (flat below C)");
    }

    // --- 4. Malformed names -> -1, and out-of-range -> -1. ---
    {
        const char* bad[] = {"", "H4", "C", "C#", "4C", "Cx4", "C4x", "C#b4", "C-", "C--1"};
        for (const char* s : bad) CHECK(noteNameToMidi(s) == -1, "malformed note name rejected");
        CHECK(noteNameToMidi("C10") == -1, "C10 (=132) is out of the MIDI range");
        CHECK(noteNameToMidi("C-2") == -1, "C-2 (=-12) is out of the MIDI range");
    }

    // --- 5. MIDI -> name (sharps). ---
    {
        CHECK(midiToNoteName(69) == "A4", "69 -> A4");
        CHECK(midiToNoteName(60) == "C4", "60 -> C4");
        CHECK(midiToNoteName(61) == "C#4", "61 -> C#4");
        CHECK(midiToNoteName(0) == "C-1", "0 -> C-1");
        CHECK(midiToNoteName(127) == "G9", "127 -> G9");
        CHECK(midiToNoteName(128).empty() && midiToNoteName(-1).empty(), "out-of-range midi -> empty name");
    }

    // --- 6. Convenience name -> frequency. ---
    {
        CHECK(close(noteNameToFrequency("A4"), 440.0, 1e-3), "A4 -> 440 Hz");
        CHECK(noteNameToFrequency("nonsense") == 0.0f, "invalid name -> 0 Hz");
    }

    if (g_fail == 0) {
        std::printf("musictheory: OK — midi<->freq A440, round-trip, note-name parse/validate, midi->name, convenience.\n");
        return 0;
    }
    std::printf("musictheory: %d failure(s).\n", g_fail);
    return 1;
}
