#pragma once

#include <string>

namespace maz::audio {

class Sequencer;

// Import a Standard MIDI File (format 0 or 1) into the sequencer's *current* pattern: melodic note
// events (any non-percussion channel) become piano-roll notes on the lead lane, and channel-10
// (index 9) General-MIDI percussion maps back onto the drum grid. Ticks are converted to steps using
// the file's division and the sequencer's current steps-per-beat. Clears the current roll + grid
// first. Returns false + sets *err on a read/parse failure. Complements writeMidi (MidiWriter).
bool readMidi(const std::string& path, Sequencer& seq, std::string* err = nullptr);

} // namespace maz::audio
