#pragma once

#include "maz/audio/LFO.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace maz::audio {

class AudioEngine;

// Parameters an automation lane can drive. Each maps to one knob elsewhere in the engine.
// (Append new targets before Count so existing saved indices stay stable.)
enum class AutoTarget {
    FilterCutoff,
    FmIndex,
    ReverbMix,
    MasterGain,
    DelayMix,
    DistDrive,
    StereoWidth,
    SynthCutoff,
    FilterResonance,
    LeadVolume,
    LeadPan,
    Count
};

// One breakpoint in an automation clip: a unipolar value in [0, 1] at a time in seconds.
struct AutoPoint {
    double time = 0.0;
    float value = 0.0f;
};

// One automation lane driving a target parameter between [lo, hi]. It has two sources: an LFO
// (periodic) and an optional **automation clip** — a list of user-drawn breakpoints (FL-style),
// linearly interpolated and looped over `clipLength`. If the clip has points it takes priority over
// the LFO; otherwise the LFO drives the lane. Disabled by default.
struct AutoLane {
    bool enabled = false;
    LFO lfo{};
    float lo = 0.0f;
    float hi = 1.0f;
    bool sync = false;  // tempo-sync the LFO rate to the transport (ignored when a clip is present)
    int syncDiv = 2;    // note-division index (default 1 bar) when sync is on

    std::vector<AutoPoint> clip{};  // breakpoints, ascending in time; empty → use the LFO
    double clipLength = 0.0;        // loop length in seconds; <=0 → hold the last point past its time

    // Evaluate the lane's *unipolar* [0,1] source value at time t (before the lo/hi remap). Uses the
    // clip if it has points, otherwise the LFO.
    float sourceUnipolar(double t) const;
};

// A small fixed bank of automation lanes — one per target. Each block, apply() evaluates every
// enabled lane at the current transport time and writes the swept value onto its target knob, so a
// filter can wobble, the FM index can breathe, etc. Deterministic (time-driven).
class Automation {
public:
    Automation();

    static constexpr int count() { return static_cast<int>(AutoTarget::Count); }
    static const char* targetName(AutoTarget t);

    // Tempo-sync note divisions for a lane's LFO (4 bars … 1/8). syncRateHz converts a division +
    // BPM into the LFO rate in Hz.
    static constexpr int kSyncDivisions = 6;
    static const char* syncDivisionName(int div);
    static float syncRateHz(int div, double bpm);

    AutoLane& lane(AutoTarget t) { return lanes_[static_cast<size_t>(t)]; }
    const AutoLane& lane(AutoTarget t) const { return lanes_[static_cast<size_t>(t)]; }
    AutoLane& lane(int i) { return lanes_[static_cast<size_t>(i)]; }

    // Whether any lane is active (so callers can skip the pass entirely).
    bool anyEnabled() const;

    // Evaluate enabled lanes at `timeSeconds` and set their target parameters on the engine. `bpm`
    // (> 0) lets tempo-synced lanes lock their LFO rate to the transport.
    void apply(AudioEngine& engine, double timeSeconds, double bpm = 0.0);

private:
    std::array<AutoLane, static_cast<size_t>(AutoTarget::Count)> lanes_{};
};

} // namespace maz::audio
