#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::anim {

// Root motion — Godot's AnimationMixer root-motion track. A locomotion clip (walk, run, roll) that
// actually TRAVELS bakes the character's displacement into a "root" bone; without root motion you play
// the clip in place and move the character with a separate hand-tuned velocity, and the feet slide
// whenever the two disagree. Root motion instead READS the travel back out of the clip and hands it to
// the character each frame, so the body moves exactly as far as the animation says — no foot sliding.
//
// This is the reusable core: a track of the root's cumulative planar POSITION and HEADING over the clip.
// `delta` returns how far the root moved between two clip times (with one-loop wrap-around), and
// `advance` applies that step to a world pose — rotating the clip-local displacement by the character's
// current facing (so "walk forward" goes wherever the character faces) and accumulating the turn. Pure
// planar math (the XZ-plane + Y-yaw reduction Godot uses for characters), so it unit-tests headless and
// stays deterministic under the fixed timestep. Heading is stored UNWRAPPED (a cumulative path integral),
// so a clip may turn any amount and deltas never need angle-wrap fixups.

struct RootMotionSample {
    math::vec2 position{0.0f, 0.0f}; // clip-local displacement (or, from sample(), cumulative position)
    float heading = 0.0f;            // radians, cumulative (unwrapped)
};

struct RootMotionKey {
    float time = 0.0f;
    math::vec2 position{0.0f, 0.0f}; // cumulative clip-local position at this time
    float heading = 0.0f;            // cumulative heading at this time (radians, unwrapped)
};

class RootMotionTrack {
public:
    // Keys are expected in increasing time; the first key's time is the clip start, the last the end.
    void addKey(float time, math::vec2 position, float heading) {
        m_keys.push_back(RootMotionKey{time, position, heading});
    }

    void clear() { m_keys.clear(); }
    std::size_t keyCount() const { return m_keys.size(); }
    float startTime() const { return m_keys.empty() ? 0.0f : m_keys.front().time; }
    float endTime() const { return m_keys.empty() ? 0.0f : m_keys.back().time; }
    float duration() const { return endTime() - startTime(); }

    // Cumulative root pose at clip time `t`, clamped to the clip's time range and linearly interpolated.
    RootMotionSample sample(float t) const {
        RootMotionSample out;
        if (m_keys.empty()) {
            return out;
        }
        if (t <= m_keys.front().time) {
            return {m_keys.front().position, m_keys.front().heading};
        }
        if (t >= m_keys.back().time) {
            return {m_keys.back().position, m_keys.back().heading};
        }
        for (std::size_t i = 1; i < m_keys.size(); ++i) {
            const RootMotionKey& b = m_keys[i];
            if (t <= b.time) {
                const RootMotionKey& a = m_keys[i - 1];
                const float span = b.time - a.time;
                const float u = span > 1e-9f ? (t - a.time) / span : 0.0f;
                out.position = a.position + (b.position - a.position) * u;
                out.heading = a.heading + (b.heading - a.heading) * u; // plain lerp: heading is cumulative
                return out;
            }
        }
        return {m_keys.back().position, m_keys.back().heading};
    }

    // Root displacement (clip-local position delta + heading delta) between `prevT` and `curT`. When
    // `loop` is set and the clip wrapped this step (curT < prevT), the two arcs — prev→end and start→cur —
    // are summed, so a looping walk keeps travelling smoothly across the loop seam.
    RootMotionSample delta(float prevT, float curT, bool loop) const {
        if (m_keys.empty()) {
            return {};
        }
        if (loop && curT < prevT) {
            const RootMotionSample toEnd = sub(sample(endTime()), sample(prevT));
            const RootMotionSample fromStart = sub(sample(curT), sample(startTime()));
            return {toEnd.position + fromStart.position, toEnd.heading + fromStart.heading};
        }
        return sub(sample(curT), sample(prevT));
    }

    // Apply one step of the clip to a world pose: the clip-local displacement is rotated into the world
    // by the character's CURRENT heading (so forward motion follows facing), then the heading turns.
    void advance(math::vec2& worldPos, float& worldHeading, float prevT, float curT, bool loop) const {
        const RootMotionSample d = delta(prevT, curT, loop);
        const float c = std::cos(worldHeading);
        const float s = std::sin(worldHeading);
        worldPos.x += d.position.x * c - d.position.y * s;
        worldPos.y += d.position.x * s + d.position.y * c;
        worldHeading += d.heading;
    }

private:
    static RootMotionSample sub(const RootMotionSample& a, const RootMotionSample& b) {
        return {a.position - b.position, a.heading - b.heading};
    }

    std::vector<RootMotionKey> m_keys;
};

} // namespace maz::anim
