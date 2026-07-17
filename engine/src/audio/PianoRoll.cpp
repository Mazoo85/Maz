#include "maz/audio/PianoRoll.hpp"

namespace maz::audio {

bool PianoRoll::hasNote(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return true;
        }
    }
    return false;
}

int PianoRoll::addChord(int startStep, int lengthSteps, int rootPitch, Chord chord, float velocity) {
    // Semitone offsets from the root for each chord quality.
    std::vector<int> offsets;
    switch (chord) {
    case Chord::Major:
        offsets = {0, 4, 7};
        break;
    case Chord::Minor:
        offsets = {0, 3, 7};
        break;
    case Chord::Dom7:
        offsets = {0, 4, 7, 10};
        break;
    case Chord::Maj7:
        offsets = {0, 4, 7, 11};
        break;
    case Chord::Min7:
        offsets = {0, 3, 7, 10};
        break;
    case Chord::Dim:
        offsets = {0, 3, 6};
        break;
    case Chord::Aug:
        offsets = {0, 4, 8};
        break;
    case Chord::Sus2:
        offsets = {0, 2, 7};
        break;
    case Chord::Sus4:
        offsets = {0, 5, 7};
        break;
    }
    for (int off : offsets) {
        Note n;
        n.startStep = startStep;
        n.lengthSteps = lengthSteps < 1 ? 1 : lengthSteps;
        n.pitch = rootPitch + off;
        n.velocity = velocity;
        notes_.push_back(n);
    }
    return static_cast<int>(offsets.size());
}

float PianoRoll::setNoteProbability(int pitch, int step, float probability) {
    const float p = probability < 0.0f ? 0.0f : (probability > 1.0f ? 1.0f : probability);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.probability = p;
            return p;
        }
    }
    return 1.0f;
}

float PianoRoll::noteProbability(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.probability;
        }
    }
    return 1.0f;
}

int PianoRoll::quantize(int division) {
    if (division < 2) {
        return 0; // 1 (or less) → already on the grid
    }
    int moved = 0;
    for (Note& n : notes_) {
        const int snapped = ((n.startStep + division / 2) / division) * division;
        if (snapped != n.startStep) {
            n.startStep = snapped;
            ++moved;
        }
    }
    return moved;
}

void PianoRoll::toggle(int pitch, int step, float velocity) {
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].pitch == pitch && notes_[i].startStep == step) {
            notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
    Note n;
    n.startStep = step;
    n.lengthSteps = 1;
    n.pitch = pitch;
    n.velocity = velocity;
    notes_.push_back(n);
}

} // namespace maz::audio
