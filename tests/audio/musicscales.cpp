// tests/audio/musicscales.cpp — verifies the scale/chord note-set builders (audio::scaleIntervals / chordIntervals
// / scaleNotes / chordNotes). Ground truths: canonical interval tables (C major degrees, minor pentatonic, the
// modes), multi-octave expansion repeats a perfect octave higher, chords are the right triads/sevenths, roots
// transpose correctly, and octaves<1 -> empty. Values checked against music theory. Also composes with
// MusicTheory (a chord's notes convert to frequencies). Pure CPU.
#include "maz/audio/MusicScales.hpp"
#include "maz/audio/MusicTheory.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::audio;

int main() {
    // --- 1. Scale degrees (C = MIDI 60). ---
    {
        const std::vector<int> cmaj = scaleNotes(60, Scale::Major, 1);
        CHECK((cmaj == std::vector<int>{60, 62, 64, 65, 67, 69, 71}), "C major = C D E F G A B");
        const std::vector<int> amin = scaleNotes(57, Scale::NaturalMinor, 1); // A3 = 57
        CHECK((amin == std::vector<int>{57, 59, 60, 62, 64, 65, 67}), "A natural minor = A B C D E F G");
        const std::vector<int> pent = scaleNotes(60, Scale::MinorPentatonic, 1);
        CHECK((pent == std::vector<int>{60, 63, 65, 67, 70}), "C minor pentatonic = C Eb F G Bb");
        CHECK(scaleIntervals(Scale::Chromatic).size() == 12, "chromatic has 12 degrees");
        CHECK(scaleIntervals(Scale::WholeTone) == (std::vector<int>{0, 2, 4, 6, 8, 10}), "whole-tone steps by 2");
    }

    // --- 2. Multi-octave expansion. ---
    {
        const std::vector<int> two = scaleNotes(60, Scale::Major, 2);
        CHECK(two.size() == 14, "two octaves of a 7-note scale = 14 notes");
        CHECK(two[7] == 72 && two[13] == 83, "second octave is a perfect octave (+12) higher");
        CHECK(scaleNotes(60, Scale::Major, 0).empty(), "octaves < 1 -> empty");
    }

    // --- 3. Chords. ---
    {
        CHECK((chordNotes(60, Chord::Major) == std::vector<int>{60, 64, 67}), "C major triad = C E G");
        CHECK((chordNotes(60, Chord::Minor) == std::vector<int>{60, 63, 67}), "C minor triad = C Eb G");
        CHECK((chordNotes(60, Chord::Diminished) == std::vector<int>{60, 63, 66}), "C dim = C Eb Gb");
        CHECK((chordNotes(60, Chord::Augmented) == std::vector<int>{60, 64, 68}), "C aug = C E G#");
        CHECK((chordNotes(60, Chord::Dominant7) == std::vector<int>{60, 64, 67, 70}), "C7 = C E G Bb");
        CHECK((chordNotes(60, Chord::Major7) == std::vector<int>{60, 64, 67, 71}), "Cmaj7 = C E G B");
        CHECK((chordNotes(60, Chord::Minor7) == std::vector<int>{60, 63, 67, 70}), "Cm7 = C Eb G Bb");
        CHECK((chordNotes(62, Chord::Minor) == std::vector<int>{62, 65, 69}), "D minor transposes (D F A)");
        CHECK((chordNotes(60, Chord::Dominant9).size() == 5), "dominant 9th has 5 notes");
    }

    // --- 4. Composability: a chord's MIDI notes convert to ascending frequencies. ---
    {
        const std::vector<int> gmaj = chordNotes(noteNameToMidi("G4"), Chord::Major);
        CHECK(gmaj.size() == 3, "G major triad built from a parsed note name");
        bool ascending = true;
        for (std::size_t i = 1; i < gmaj.size(); ++i)
            if (midiToFrequency(gmaj[i]) <= midiToFrequency(gmaj[i - 1])) ascending = false;
        CHECK(ascending, "the chord's notes convert to strictly ascending frequencies");
        // Root of G major is G4 = 392 Hz (approx).
        CHECK(std::abs(midiToFrequency(gmaj[0]) - 392.0f) < 0.5f, "G4 root ~ 392 Hz");
    }

    if (g_fail == 0) {
        std::printf("musicscales: OK — scale degrees, multi-octave, triads/7ths/ext, transpose, freq composability.\n");
        return 0;
    }
    std::printf("musicscales: %d failure(s).\n", g_fail);
    return 1;
}
