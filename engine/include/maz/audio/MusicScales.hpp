#pragma once

#include <vector>

// maz::audio SCALES & CHORDS — build the note sets procedural music needs from a root note: a scale (major, the
// modes, pentatonics, blues, whole-tone, chromatic) or a chord (triads, sevenths, sus, extensions). Returns MIDI
// note numbers you feed to `midiToFrequency` (MusicTheory.hpp) → `audio::Oscillator` for an arpeggiator, a
// generative melody line, or chord stabs. The interval tables are the canonical semitone offsets from the root.
// Header-only, std-only, deterministic. Complements the note↔pitch conversions; together they're a small
// music-theory toolkit the audio module previously lacked.
namespace maz::audio {

enum class Scale {
    Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    MajorPentatonic, MinorPentatonic, Blues, WholeTone, Chromatic
};

enum class Chord {
    Major, Minor, Diminished, Augmented, Sus2, Sus4,
    Major7, Minor7, Dominant7, Diminished7, HalfDiminished7, MinorMajor7,
    Major6, Minor6, Dominant9
};

// The semitone offsets from the root for one octave of `scale` (e.g. Major → {0,2,4,5,7,9,11}).
inline std::vector<int> scaleIntervals(Scale s) {
    switch (s) {
        case Scale::Major:           return {0, 2, 4, 5, 7, 9, 11};
        case Scale::NaturalMinor:    return {0, 2, 3, 5, 7, 8, 10};
        case Scale::HarmonicMinor:   return {0, 2, 3, 5, 7, 8, 11};
        case Scale::MelodicMinor:    return {0, 2, 3, 5, 7, 9, 11};
        case Scale::Dorian:          return {0, 2, 3, 5, 7, 9, 10};
        case Scale::Phrygian:        return {0, 1, 3, 5, 7, 8, 10};
        case Scale::Lydian:          return {0, 2, 4, 6, 7, 9, 11};
        case Scale::Mixolydian:      return {0, 2, 4, 5, 7, 9, 10};
        case Scale::Locrian:         return {0, 1, 3, 5, 6, 8, 10};
        case Scale::MajorPentatonic: return {0, 2, 4, 7, 9};
        case Scale::MinorPentatonic: return {0, 3, 5, 7, 10};
        case Scale::Blues:           return {0, 3, 5, 6, 7, 10};
        case Scale::WholeTone:       return {0, 2, 4, 6, 8, 10};
        case Scale::Chromatic:       return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    }
    return {0, 2, 4, 5, 7, 9, 11};
}

// The semitone offsets from the root for `chord` (e.g. Major → {0,4,7}, Dominant7 → {0,4,7,10}).
inline std::vector<int> chordIntervals(Chord c) {
    switch (c) {
        case Chord::Major:           return {0, 4, 7};
        case Chord::Minor:           return {0, 3, 7};
        case Chord::Diminished:      return {0, 3, 6};
        case Chord::Augmented:       return {0, 4, 8};
        case Chord::Sus2:            return {0, 2, 7};
        case Chord::Sus4:            return {0, 5, 7};
        case Chord::Major7:          return {0, 4, 7, 11};
        case Chord::Minor7:          return {0, 3, 7, 10};
        case Chord::Dominant7:       return {0, 4, 7, 10};
        case Chord::Diminished7:     return {0, 3, 6, 9};
        case Chord::HalfDiminished7: return {0, 3, 6, 10};
        case Chord::MinorMajor7:     return {0, 3, 7, 11};
        case Chord::Major6:          return {0, 4, 7, 9};
        case Chord::Minor6:          return {0, 3, 7, 9};
        case Chord::Dominant9:       return {0, 4, 7, 10, 14};
    }
    return {0, 4, 7};
}

// MIDI notes for `scale` starting at `rootMidi`, spanning `octaves` octaves (each octave repeats the degrees a
// perfect octave higher). `octaves` < 1 → empty. The top octave's root is not duplicated.
inline std::vector<int> scaleNotes(int rootMidi, Scale s, int octaves = 1) {
    std::vector<int> out;
    if (octaves < 1) return out;
    const std::vector<int> iv = scaleIntervals(s);
    out.reserve(iv.size() * static_cast<std::size_t>(octaves));
    for (int o = 0; o < octaves; ++o)
        for (int step : iv) out.push_back(rootMidi + step + 12 * o);
    return out;
}

// MIDI notes for `chord` rooted at `rootMidi`.
inline std::vector<int> chordNotes(int rootMidi, Chord c) {
    std::vector<int> out;
    const std::vector<int> iv = chordIntervals(c);
    out.reserve(iv.size());
    for (int step : iv) out.push_back(rootMidi + step);
    return out;
}

} // namespace maz::audio
