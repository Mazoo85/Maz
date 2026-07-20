#pragma once

#include "maz/audio/Effect.hpp"

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
};

} // namespace maz::audio
