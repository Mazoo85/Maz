#pragma once

#include <cstdint>
#include <vector>

namespace maz::audio {

// A single note placed on the piano roll: it starts on `startStep`, lasts `lengthSteps` steps, at
// MIDI pitch `pitch`, with linear `velocity` in [0, 1].
struct Note {
    int startStep = 0;
    int lengthSteps = 1;
    int pitch = 60; // middle C
    float velocity = 0.9f;
    float probability = 1.0f; // chance in [0,1] the note fires each loop (1 = always)
};

// Common chord qualities for the chord tool. Each expands to a set of semitone offsets from the root.
enum class Chord { Major, Minor, Dom7, Maj7, Min7, Dim, Aug, Sus2, Sus4 };

// Musical scales for the scale-snap tool. Each maps to the set of semitone degrees (0..11) it allows
// above the root pitch class.
enum class Scale {
    Major,
    Minor, // natural minor (Aeolian)
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    HarmonicMinor,
    MelodicMinor,
    PentatonicMajor,
    PentatonicMinor,
    Blues
};

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

    // Quantize: snap every note's start to the nearest multiple of `division` steps (1 = no-op,
    // 4 = to the beat at 16ths). Returns the number of notes moved.
    int quantize(int division);

    // Scale-snap: move every off-scale note to the nearest pitch that belongs to `scale` rooted at
    // pitch class `rootPitch` (only the root's pitch class matters, any octave). On a tie the note
    // snaps down. Notes already in the scale are untouched. Returns the number of notes moved.
    int snapToScale(int rootPitch, Scale scale);

    // Strum: for every stack of notes that share a start step, stagger their starts so the chord
    // rolls — ordered low pitch to high, the j-th note is delayed by `stepOffset · j` steps (a
    // negative offset rolls from the top instead; starts are clamped at 0). The lowest note of each
    // stack stays put. Returns the number of notes moved.
    int strum(int stepOffset);

    // Legato: extend every note so it lasts right up to the next note's start (the nearest start step
    // greater than its own), gluing the line together with no gaps. Notes with nothing after them
    // keep their length. Returns the number of notes whose length changed.
    int legato();

    // Invert: mirror every note's pitch around `pivotPitch` (newPitch = 2·pivot − pitch), the
    // classic melodic inversion — intervals flip direction while their sizes are preserved. Pitches
    // are clamped to the MIDI range. Returns the number of notes whose pitch changed.
    int invert(int pivotPitch);

    // Reverse (flip horizontally in time): mirror every note's position within the pattern so its end
    // becomes its start (newStart = numSteps − start − length), reversing the rhythm while keeping
    // each note's length and pitch. Applying it twice restores the original. Returns the number of
    // notes moved.
    int reverseTime();

    // Duplicate: append a copy of every current note shifted later by `offsetSteps`, extending the
    // phrase (the FL "duplicate" / Ctrl+B move). offsetSteps must be > 0. Returns the number of notes
    // added.
    int duplicate(int offsetSteps);

    // Randomize (humanize) velocities: scale each note's velocity by a random factor in
    // [1−amount, 1+amount] (clamped to [0,1]), for natural-sounding dynamics. `seed` makes it fully
    // deterministic — the same seed and notes always give the same result. Returns the number of
    // notes whose velocity changed.
    int randomizeVelocity(float amount, uint32_t seed);

    // Is there any note at this exact (pitch, step) start cell? (Step-entry granularity.)
    bool hasNote(int pitch, int step) const;

    // Toggle a length-1 note at (pitch, step): remove it if present, otherwise add it.
    void toggle(int pitch, int step, float velocity = 0.9f);

    // Set the trigger probability of the note starting at (pitch, step), if one exists. Returns its
    // new probability (or 1.0 if there's no note there).
    float setNoteProbability(int pitch, int step, float probability);
    float noteProbability(int pitch, int step) const;

private:
    int numSteps_ = 16;
    int lowPitch_ = 48;   // C3
    int numPitches_ = 25; // two octaves + 1 (C3..C5)
    std::vector<Note> notes_;
};

} // namespace maz::audio
