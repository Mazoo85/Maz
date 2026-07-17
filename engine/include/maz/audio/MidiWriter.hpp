#pragma once

#include <string>

namespace maz::audio {

class Sequencer;

// Export the sequencer's current pattern to a Standard MIDI File (Type 0): the piano-roll notes on
// channel 1, plus the drum grid as General-MIDI percussion on channel 10 (velocity-aware). `ppq` is
// the timing resolution (ticks per quarter note). Returns false + sets *err on I/O failure.
bool writeMidi(const std::string& path, Sequencer& seq, int ppq = 96, std::string* err = nullptr);

} // namespace maz::audio
