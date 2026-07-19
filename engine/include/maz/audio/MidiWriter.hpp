#pragma once

#include <string>

namespace maz::audio {

class Sequencer;

// Export the sequencer to a Standard MIDI File (Type 0): the lead piano-roll on channel 0, the bass
// roll on channel 1, and the drum grid as General-MIDI percussion on channel 10 (velocity-aware,
// mapped by each channel's drum type). A tempo meta carries the project BPM. `ppq` is the timing
// resolution (ticks per quarter note). When `arrangement` is true the whole playlist is written back
// to back (each entry offset by one pattern length); otherwise just the current pattern. Returns
// false + sets *err on I/O failure.
bool writeMidi(const std::string& path, Sequencer& seq, int ppq = 96, std::string* err = nullptr,
               bool arrangement = false);

} // namespace maz::audio
