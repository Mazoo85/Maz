#pragma once

#include "maz/audio/Effects.hpp"

#include <vector>

namespace maz::audio {

// The master bus mixer: a fixed chain of built-in effects (EQ → compressor → delay → reverb) run in
// order over the stereo output, then a master gain. Individual effects are disabled by default so
// the dry signal is untouched until the user (or a demo) switches them on.
//
// Per-instrument track levels (drums, synth, tone) live upstream on the sources; this is the final
// stage every sound passes through.
class Mixer {
public:
    Mixer();

    void setMasterGain(float g) { masterGain_ = g; }
    float masterGain() const { return masterGain_; }

    // Typed access for the UI / demos.
    LowPass& eq() { return eq_; }
    Distortion& distortion() { return dist_; }
    Compressor& compressor() { return comp_; }
    Chorus& chorus() { return chorus_; }
    Delay& delay() { return delay_; }
    Reverb& reverb() { return reverb_; }

    // Generic iteration over the chain (for a mixer strip that lists every effect).
    int effectCount() const { return static_cast<int>(chain_.size()); }
    Effect& effect(int i) { return *chain_[static_cast<size_t>(i)]; }

    // Process `frames` of interleaved stereo in place: run each enabled effect, then master gain.
    void process(float* stereo, int frames, int sampleRate);

    void reset();

private:
    float masterGain_ = 0.9f;
    LowPass eq_{};
    Distortion dist_{};
    Compressor comp_{};
    Chorus chorus_{};
    Delay delay_{};
    Reverb reverb_{};
    std::vector<Effect*> chain_; // processing order; points at the members above
};

} // namespace maz::audio
