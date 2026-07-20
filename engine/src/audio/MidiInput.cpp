#include "maz/audio/MidiInput.hpp"

namespace maz::audio {

void MidiInput::pushNoteOn(int key, float velocity) {
    std::lock_guard<std::mutex> lock(mu_);
    events_.push_back({key, velocity, true});
}

void MidiInput::pushNoteOff(int key) {
    std::lock_guard<std::mutex> lock(mu_);
    events_.push_back({key, 0.0f, false});
}

std::vector<MidiInput::Event> MidiInput::drain() {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<Event> out;
    out.swap(events_);
    return out;
}

bool MidiInput::empty() const {
    std::lock_guard<std::mutex> lock(mu_);
    return events_.empty();
}

} // namespace maz::audio
