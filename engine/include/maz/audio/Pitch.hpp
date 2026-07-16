#pragma once

#include <cmath>

namespace maz::audio {

// MIDI note number → frequency in Hz (equal temperament, A4 = MIDI 69 = 440 Hz).
inline float midiToFreq(int midi) {
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

// Note names for the 12 pitch classes, indexed by (midi % 12). Sharps only (no flats).
inline const char* pitchClassName(int midi) {
    static const char* kNames[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                     "F#", "G",  "G#", "A",  "A#", "B"};
    int pc = midi % 12;
    if (pc < 0) {
        pc += 12;
    }
    return kNames[pc];
}

// The octave number for a MIDI note (C4 = middle C = MIDI 60, following the common convention).
inline int midiOctave(int midi) {
    return midi / 12 - 1;
}

} // namespace maz::audio
