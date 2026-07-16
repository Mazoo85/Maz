#pragma once

#include "maz/audio/LFO.hpp"

#include <array>
#include <cstddef>

namespace maz::audio {

class AudioEngine;

// Parameters an automation lane can drive. Each maps to one knob elsewhere in the engine.
enum class AutoTarget { FilterCutoff, FmIndex, ReverbMix, MasterGain, Count };

// One automation lane: an LFO sweeping a target parameter between [lo, hi]. Disabled by default.
struct AutoLane {
    bool enabled = false;
    LFO lfo{};
    float lo = 0.0f;
    float hi = 1.0f;
};

// A small fixed bank of automation lanes — one per target. Each block, apply() evaluates every
// enabled lane at the current transport time and writes the swept value onto its target knob, so a
// filter can wobble, the FM index can breathe, etc. Deterministic (time-driven).
class Automation {
public:
    Automation();

    static constexpr int count() { return static_cast<int>(AutoTarget::Count); }
    static const char* targetName(AutoTarget t);

    AutoLane& lane(AutoTarget t) { return lanes_[static_cast<size_t>(t)]; }
    const AutoLane& lane(AutoTarget t) const { return lanes_[static_cast<size_t>(t)]; }
    AutoLane& lane(int i) { return lanes_[static_cast<size_t>(i)]; }

    // Whether any lane is active (so callers can skip the pass entirely).
    bool anyEnabled() const;

    // Evaluate enabled lanes at `timeSeconds` and set their target parameters on the engine.
    void apply(AudioEngine& engine, double timeSeconds);

private:
    std::array<AutoLane, static_cast<size_t>(AutoTarget::Count)> lanes_{};
};

} // namespace maz::audio
