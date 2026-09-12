#pragma once

#include <cmath>
#include <string>

// maz::audio MUSIC THEORY — the note ↔ pitch conversions procedural music and synth voices need but the audio
// module lacked. Convert a MIDI note number to a frequency in Hz (12-tone equal temperament, A4 = MIDI 69 = 440 Hz)
// and back, parse scientific-pitch note names ("A4", "C#5", "Bb3", "C-1") to MIDI numbers, and render a MIDI number
// back to a name. Feed the result straight into `audio::Oscillator` / a `Sound`'s `freq`, or drive an arpeggiator /
// `MusicSequencer`. Header-only, std-only, deterministic.
//
// Scope note (honest): standard 12-TET at A4=440 Hz with C4 = middle C = MIDI 60 (scientific pitch notation);
// `noteNameToMidi` accepts one optional '#'/'b' accidental and validates the result to the MIDI range [0,127],
// returning -1 otherwise. Alternate tunings / microtonality are out of scope.
namespace maz::audio {

// MIDI note number → frequency in Hz. 69 → 440, 60 → ~261.63 (middle C), +12 doubles the pitch (one octave).
inline float midiToFrequency(int midi) {
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

// Frequency in Hz → (fractional) MIDI note number. Inverse of midiToFrequency; round for the nearest note.
inline double frequencyToMidi(double hz) {
    if (hz <= 0.0) return 0.0;
    return 69.0 + 12.0 * std::log2(hz / 440.0);
}

// Parse a scientific-pitch note name ("C4", "A4", "C#5", "Db3", "C-1") to a MIDI number, or -1 if malformed or out
// of the MIDI range. Letter A–G (case-insensitive), an optional '#'/'b', then a (possibly negative) octave.
inline int noteNameToMidi(const std::string& name) {
    std::size_t i = 0, n = name.size();
    if (i >= n) return -1;
    int semitone;
    switch (name[i]) {
        case 'C': case 'c': semitone = 0; break;
        case 'D': case 'd': semitone = 2; break;
        case 'E': case 'e': semitone = 4; break;
        case 'F': case 'f': semitone = 5; break;
        case 'G': case 'g': semitone = 7; break;
        case 'A': case 'a': semitone = 9; break;
        case 'B': case 'b': semitone = 11; break;
        default: return -1;
    }
    ++i;
    if (i < n && (name[i] == '#' || name[i] == 'b')) {
        semitone += (name[i] == '#') ? 1 : -1;
        ++i;
    }
    if (i >= n) return -1; // octave is mandatory
    bool neg = false;
    if (name[i] == '-') { neg = true; ++i; }
    if (i >= n) return -1; // dangling sign
    int oct = 0;
    std::size_t digits = 0;
    for (; i < n; ++i) {
        if (name[i] < '0' || name[i] > '9') return -1; // trailing junk
        oct = oct * 10 + (name[i] - '0');
        ++digits;
        if (oct > 1000) oct = 1000;
    }
    if (digits == 0) return -1;
    if (neg) oct = -oct;
    const int midi = (oct + 1) * 12 + semitone;
    return (midi >= 0 && midi <= 127) ? midi : -1;
}

// Render a MIDI number back to a scientific-pitch name using sharps ("C4", "C#4", "A4"). Out-of-range → "".
inline std::string midiToNoteName(int midi) {
    if (midi < 0 || midi > 127) return "";
    static const char* const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int oct = midi / 12 - 1;
    return std::string(names[midi % 12]) + std::to_string(oct);
}

// Convenience: note name straight to frequency in Hz (0 if the name is invalid).
inline float noteNameToFrequency(const std::string& name) {
    const int midi = noteNameToMidi(name);
    return midi < 0 ? 0.0f : midiToFrequency(midi);
}

} // namespace maz::audio
