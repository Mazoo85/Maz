#pragma once

#include <vector>

namespace maz::audio {

// A single note placed on the piano roll: it starts on `startStep`, lasts `lengthSteps` steps, at
// MIDI pitch `pitch`, with linear `velocity` in [0, 1].
struct Note {
    int startStep = 0;
    int lengthSteps = 1;
    int pitch = 60; // middle C
    float velocity = 0.9f;
};

// Common chord qualities for the chord tool. Each expands to a set of semitone offsets from the root.
enum class Chord { Major, Minor, Dom7, Maj7, Min7, Dim, Aug, Sus2, Sus4 };

// The melodic pattern the piano roll edits and the sequencer plays: a set of notes over a step
// timeline, addressed by a visible pitch window (a contiguous range of MIDI notes shown as rows).
// Step entry uses length-1 notes; the model supports longer notes for later editing.
class PianoRoll {
public:
    int numSteps() const { return numSteps_; }
    int lowPitch() const { return lowPitch_; }   // MIDI note of the bottom row
    int numPitches() const { return numPitches_; } // number of rows shown
    int highPitch() const { return lowPitch_ + numPitches_ - 1; }

    const std::vector<Note>& notes() const { return notes_; }

    void clear() { notes_.clear(); }
    void addNote(const Note& n) { notes_.push_back(n); }

    // Chord tool: add every note of `chord` (rooted at MIDI `rootPitch`) as one block starting at
    // `startStep` for `lengthSteps`. Returns the number of notes added.
    int addChord(int startStep, int lengthSteps, int rootPitch, Chord chord, float velocity = 0.9f);

    // Is there any note at this exact (pitch, step) start cell? (Step-entry granularity.)
    bool hasNote(int pitch, int step) const;

    // Toggle a length-1 note at (pitch, step): remove it if present, otherwise add it.
    void toggle(int pitch, int step, float velocity = 0.9f);

private:
    int numSteps_ = 16;
    int lowPitch_ = 48;   // C3
    int numPitches_ = 25; // two octaves + 1 (C3..C5)
    std::vector<Note> notes_;
};

} // namespace maz::audio
