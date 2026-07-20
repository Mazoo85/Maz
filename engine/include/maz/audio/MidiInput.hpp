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
        enum class Type { NoteOn, NoteOff, ControlChange };
        Type type;
        int index;    // MIDI note number (notes) or controller number (control change), 0..127
        float value;  // note velocity 0..1 (NoteOn) or controller value 0..1 (ControlChange)
    };

    // Queue an event (safe to call from a device/callback thread).
    void pushNoteOn(int key, float velocity);
    void pushNoteOff(int key);
    // A MIDI continuous-controller message: controller number + normalised value 0..1. Used for
    // live "MIDI-learn" parameter control (map a knob/slider to an engine parameter).
    void pushControlChange(int controller, float value);

    // Move all queued events out for processing, clearing the queue. Called once per render block.
    std::vector<Event> drain();

    bool empty() const;

private:
    mutable std::mutex mu_;
    std::vector<Event> events_;
};

} // namespace maz::audio
