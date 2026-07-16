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
