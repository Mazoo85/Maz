#pragma once

#include "maz/anim/Tween.hpp" // Loop

#include <cstddef>
#include <vector>

namespace maz::anim {

// Call-method / trigger tracks — the other half of Godot's AnimationPlayer (M100's Timeline gave VALUE
// tracks that interpolate a property; this gives METHOD tracks that FIRE at a keyframe time). A trigger
// track is a list of timed markers; as a playhead sweeps across the clip each marker fires EXACTLY ONCE
// when the head passes it — the hook a clip uses to play a footstep sound on the plant frame, spawn a
// muzzle flash on the shoot frame, or open a gate at the end of a cutscene. Pure timing math (no GPU),
// deterministic under the fixed timestep, so it unit-tests headless.

struct Trigger {
    float time = 0.0f;
    int id = 0; // caller-defined: which method/event this marker fires
};

// A sorted list of timed markers. collectRange reports the markers a monotonic forward sweep crossed.
class TriggerTrack {
public:
    // Insert a marker, keeping the list sorted by time (stable for equal times).
    void add(float time, int id) {
        Trigger t{time, id};
        auto it = triggers_.begin();
        while (it != triggers_.end() && it->time <= time) {
            ++it;
        }
        triggers_.insert(it, t);
    }

    std::size_t size() const { return triggers_.size(); }
    const Trigger& at(std::size_t i) const { return triggers_[i]; }
    float endTime() const { return triggers_.empty() ? 0.0f : triggers_.back().time; }

    // Append the ids of markers a FORWARD sweep from `from` to `to` crossed, in time order. The interval
    // is half-open [from, to) by default so a marker sitting exactly on a segment boundary fires once
    // (on the segment that starts there) and never twice. Set `includeTo` for the final segment of a
    // non-looping clip, so a marker sitting exactly at the clip end still fires.
    void collectRange(float from, float to, bool includeTo, std::vector<int>& out) const {
        for (const Trigger& t : triggers_) {
            const bool afterStart = t.time >= from;
            const bool beforeEnd = includeTo ? (t.time <= to) : (t.time < to);
            if (afterStart && beforeEnd) {
                out.push_back(t.id);
            }
        }
    }

private:
    std::vector<Trigger> triggers_;
};

// Plays a TriggerTrack over a clip, reporting the ids fired each update. Handles the loop wrap for
// Repeat (fire the tail of the clip, wrap, then keep firing) and terminates for Once. Markers should
// live in [0, length); a Repeat marker exactly at `length` is the next loop's 0 and is not double-fired.
class MethodTimeline {
public:
    TriggerTrack track;
    float length = 0.0f; // clip length; 0 => use the track's last marker time
    Loop loop = Loop::Once;
    float time = 0.0f;
    bool finished = false;

    float clipLength() const { return length > 0.0f ? length : track.endTime(); }

    void reset() {
        time = 0.0f;
        finished = false;
    }

    // Advance the playhead by dt, appending every trigger id that fired (in fire order) to `out`.
    void update(float dt, std::vector<int>& out) {
        const float len = clipLength();
        if (finished || len <= 0.0f || dt <= 0.0f) {
            return;
        }
        float remaining = dt;
        int guard = 0;
        while (remaining > 0.0f && guard++ < 100000) {
            const float toEnd = len - time;
            if (remaining < toEnd) {
                track.collectRange(time, time + remaining, false, out);
                time += remaining;
                remaining = 0.0f;
            } else if (loop == Loop::Once) {
                track.collectRange(time, len, true, out); // inclusive: fire an end-of-clip marker
                time = len;
                finished = true;
                return;
            } else { // Repeat: fire to the end (half-open), wrap to 0, continue
                track.collectRange(time, len, false, out);
                remaining -= toEnd;
                time = 0.0f;
            }
        }
    }
};

} // namespace maz::anim
