#include "maz/audio/MidiInput.hpp"

namespace maz::audio {

void MidiInput::pushNoteOn(int key, float velocity) {
    std::lock_guard<std::mutex> lock(mu_);
    events_.push_back({Event::Type::NoteOn, key, velocity});
}

void MidiInput::pushNoteOff(int key) {
    std::lock_guard<std::mutex> lock(mu_);
    events_.push_back({Event::Type::NoteOff, key, 0.0f});
}

void MidiInput::pushControlChange(int controller, float value) {
    std::lock_guard<std::mutex> lock(mu_);
    events_.push_back({Event::Type::ControlChange, controller, value});
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
