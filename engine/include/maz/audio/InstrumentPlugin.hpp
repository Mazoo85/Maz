#pragma once

#include "maz/audio/Effect.hpp"

#include <string>

namespace maz::audio {

// Common interface for a hosted plugin played as an *instrument* — driven by note events rather than
// modulating incoming audio. Both ClapHost and Vst3Host implement it, so the Sequencer can host either
// format on a channel behind one pointer type (the concrete host is chosen by the plugin file's
// extension). It refines Effect (so a hosted instrument still runs through the same process() path)
// with the note-driving methods every instrument host shares.
class InstrumentPlugin : public Effect {
public:
    // True once a plugin module is loaded and ready to process.
    virtual bool loaded() const = 0;

    // Queue a note-on / note-off for the next process() call (velocity 0..1). The host delivers them
    // to the plugin as format-native events at the top of that block.
    virtual void noteOn(int key, float velocity) = 0;
    virtual void noteOff(int key) = 0;
    // Release every currently-held note (panic) so a hosted instrument doesn't hang on stop.
    virtual void allNotesOff() = 0;

    // Automatable parameters exposed by the hosted plugin. Default: none, so a host that doesn't
    // implement parameters (or a plugin that exposes none) is unaffected. `setParam` queues a
    // parameter change delivered to the plugin at the top of the next process() block (like notes);
    // `value` is in the parameter's own range. `paramName` is for the UI.
    virtual int paramCount() const { return 0; }
    virtual void setParam(int index, double value) {
        (void)index;
        (void)value;
    }
    virtual double paramValue(int index) const {
        (void)index;
        return 0.0;
    }
    virtual double paramMin(int index) const {
        (void)index;
        return 0.0;
    }
    virtual double paramMax(int index) const {
        (void)index;
        return 1.0;
    }
    virtual std::string paramName(int index) const {
        (void)index;
        return "";
    }
};

} // namespace maz::audio
