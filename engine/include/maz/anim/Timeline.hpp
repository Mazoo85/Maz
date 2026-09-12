#pragma once

#include "maz/anim/Tween.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace maz::anim {

// ---- Keyframe timeline / sequencer -------------------------------------------------------------
// Godot's AnimationPlayer in miniature: an animation is a set of named TRACKS, each a list of
// KEYFRAMES (time -> value) that are interpolated between, and a PLAYHEAD that advances over the
// clip's length with a loop policy. Where a `Tween` (see Tween.hpp) animates ONE value from A to B,
// a `Timeline` animates MANY named properties through arbitrary keyed poses at once — the backbone of
// cutscenes, UI transitions, and property animation. Pure math (no GPU/allocation beyond the key
// vectors), so it unit-tests headless and samples identically every frame under the fixed timestep.

// One keyframe: a value at a time, plus the easing used to reach the NEXT keyframe.
struct Keyframe {
    float time = 0.0f;
    float value = 0.0f;
    Ease ease = Ease::Linear; // interpolation curve from this key to the following one
};

// A single animated channel: keyframes kept sorted by time. sample() holds the endpoints (no
// extrapolation) and eases between neighbouring keys inside the range.
struct Track {
    std::vector<Keyframe> keys;

    // Insert a key, keeping `keys` sorted by time (stable for equal times).
    void add(float time, float value, Ease ease = Ease::Linear) {
        Keyframe k{time, value, ease};
        auto it = keys.begin();
        while (it != keys.end() && it->time <= time) {
            ++it;
        }
        keys.insert(it, k);
    }

    float endTime() const { return keys.empty() ? 0.0f : keys.back().time; }

    float sample(float t) const {
        if (keys.empty()) {
            return 0.0f;
        }
        if (t <= keys.front().time) {
            return keys.front().value; // hold before the first key
        }
        if (t >= keys.back().time) {
            return keys.back().value; // hold after the last key
        }
        // Find the segment [keys[i], keys[i+1]) that contains t.
        std::size_t i = 0;
        while (i + 1 < keys.size() && keys[i + 1].time <= t) {
            ++i;
        }
        const Keyframe& a = keys[i];
        const Keyframe& b = keys[i + 1];
        const float span = b.time - a.time;
        const float local = span > 1e-8f ? (t - a.time) / span : 0.0f;
        return mix(a.value, b.value, ease(a.ease, local));
    }
};

// A named collection of tracks plus a playhead. `duration` of 0 means "auto" — the length of the
// longest track. Advance the playhead with update(dt) under a Loop policy, then read value(name).
struct Timeline {
    struct NamedTrack {
        std::string name;
        Track track;
    };

    std::vector<NamedTrack> tracks;
    float duration = 0.0f; // 0 = auto (longest track's end time)
    Loop loop = Loop::Once;
    float time = 0.0f;      // elapsed time within the clip
    bool finished = false;  // set only for Loop::Once at the end
    bool reversing = false; // PingPong: currently travelling backward

    // Get (or lazily create) a track by name.
    Track& track(const std::string& name) {
        for (auto& nt : tracks) {
            if (nt.name == name) {
                return nt.track;
            }
        }
        tracks.push_back({name, Track{}});
        return tracks.back().track;
    }

    // Effective clip length.
    float length() const {
        if (duration > 0.0f) {
            return duration;
        }
        float m = 0.0f;
        for (const auto& nt : tracks) {
            m = std::max(m, nt.track.endTime());
        }
        return m;
    }

    void reset() {
        time = 0.0f;
        finished = false;
        reversing = false;
    }

    void update(float dt) {
        const float len = length();
        if (finished || len <= 0.0f) {
            if (len <= 0.0f) {
                finished = loop == Loop::Once;
            }
            return;
        }
        time += dt;
        while (time >= len) {
            switch (loop) {
                case Loop::Once:
                    time = len;
                    finished = true;
                    return;
                case Loop::Repeat:
                    time -= len;
                    break;
                case Loop::PingPong:
                    time -= len;
                    reversing = !reversing;
                    break;
            }
        }
    }

    // The query time the playhead maps to (ping-pong reflects the clip when travelling backward).
    float playhead() const {
        const float len = length();
        if (len <= 0.0f) {
            return 0.0f;
        }
        return reversing ? len - time : time;
    }

    // Sample a named track at the current playhead. Unknown track -> 0.
    float value(const std::string& name) const {
        const float qt = playhead();
        for (const auto& nt : tracks) {
            if (nt.name == name) {
                return nt.track.sample(qt);
            }
        }
        return 0.0f;
    }

    // Sample a named track at an explicit time (ignores the playhead). Unknown track -> 0.
    float valueAt(const std::string& name, float t) const {
        for (const auto& nt : tracks) {
            if (nt.name == name) {
                return nt.track.sample(t);
            }
        }
        return 0.0f;
    }
};

} // namespace maz::anim
