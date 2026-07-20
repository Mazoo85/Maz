#pragma once

#include <mutex>
#include <vector>

namespace maz::audio {

// A thread-safe queue of incoming live MIDI note events. A device backend (ALSA / RtMidi / an OS MIDI
// callback) pushes events from its own thread as they arrive; the audio render thread drains them once
// per block and routes them to the live instrument, so you can play the keyboard on top of a running
// sequence. The queue is deliberately backend-agnostic: it has no device dependency, so the live-input
// routing is fully testable headlessly by pushing events directly (no MIDI hardware required). Wiring a
// real device backend later just means calling pushNoteOn/pushNoteOff from its callback.
class MidiInput {
public:
    struct Event {
        int key;         // MIDI note number 0..127
        float velocity;  // 0..1 (ignored for note-off)
        bool on;         // true = note-on, false = note-off
    };

    // Queue a note event (safe to call from a device/callback thread).
    void pushNoteOn(int key, float velocity);
    void pushNoteOff(int key);

    // Move all queued events out for processing, clearing the queue. Called once per render block.
    std::vector<Event> drain();

    bool empty() const;

private:
    mutable std::mutex mu_;
    std::vector<Event> events_;
};

} // namespace maz::audio
