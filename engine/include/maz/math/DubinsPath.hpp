#pragma once

#include "maz/math/Math.hpp" // vec2

#include <array>
#include <cmath>

// maz::math Dubins path — the SHORTEST path for a forward-only vehicle with a minimum turning radius, from a
// start pose (position + heading) to a goal pose. A Dubins car can drive straight or turn left/right at a
// fixed radius but never reverse; the optimum is always one of six primitive words — three arc/straight/arc
// (CSC: LSL, LSR, RSL, RSR) and two arc/arc/arc (CCC: RLR, LRL) — and the shortest valid one is the answer.
// This is the standard motion primitive for steering cars, boats, planes and any agent that "can't turn on a
// dime": planning a lane change, an approach curve, a patrol turn, or the reference path a pursuit controller
// tracks. Godot has no Dubins/curvature-constrained planner. Returns the chosen word, its three segment
// lengths, and a sampler that walks the path — following the returned controls lands exactly on the goal.
// Deterministic, header-only, std-only.
namespace maz::math {

enum class DubinsSeg { Left, Straight, Right };
enum class DubinsWord { LSL, LSR, RSL, RSR, RLR, LRL };

struct Pose2 {
    vec2 pos{0.0f};
    float heading = 0.0f; // radians
};

struct DubinsPath {
    bool ok = false;
    float length = 0.0f;               // total path length in world units (>= straight-line distance)
    DubinsWord word = DubinsWord::LSL; // which primitive won
    std::array<DubinsSeg, 3> seg{DubinsSeg::Straight, DubinsSeg::Straight, DubinsSeg::Straight};
    std::array<float, 3> segLen{0.0f, 0.0f, 0.0f}; // segment lengths, NORMALISED (radius = 1); *radius = world
    float radius = 1.0f;
    Pose2 start;
};

namespace dubins_detail {
inline float mod2pi(float x) {
    const float twoPi = 6.28318530717958647692f;
    float r = std::fmod(x, twoPi);
    if (r < 0.0f) {
        r += twoPi;
    }
    return r;
}

// The three segment types of each word, in order.
inline std::array<DubinsSeg, 3> wordSegs(DubinsWord w) {
    switch (w) {
        case DubinsWord::LSL: return {DubinsSeg::Left, DubinsSeg::Straight, DubinsSeg::Left};
        case DubinsWord::LSR: return {DubinsSeg::Left, DubinsSeg::Straight, DubinsSeg::Right};
        case DubinsWord::RSL: return {DubinsSeg::Right, DubinsSeg::Straight, DubinsSeg::Left};
        case DubinsWord::RSR: return {DubinsSeg::Right, DubinsSeg::Straight, DubinsSeg::Right};
        case DubinsWord::RLR: return {DubinsSeg::Right, DubinsSeg::Left, DubinsSeg::Right};
        case DubinsWord::LRL: return {DubinsSeg::Left, DubinsSeg::Right, DubinsSeg::Left};
    }
    return {DubinsSeg::Straight, DubinsSeg::Straight, DubinsSeg::Straight};
}

// Solve one word for normalised segment lengths (t, p, q) given the normalised geometry (alpha, beta, d).
// Returns false if the word has no solution for this geometry. (Shkel & Lumelsky / A. Walker formulation.)
inline bool solveWord(DubinsWord w, float alpha, float beta, float d, std::array<float, 3>& out) {
    const float sa = std::sin(alpha), sb = std::sin(beta);
    const float ca = std::cos(alpha), cb = std::cos(beta);
    const float cab = std::cos(alpha - beta);
    switch (w) {
        case DubinsWord::LSL: {
            const float p2 = 2.0f + d * d - 2.0f * cab + 2.0f * d * (sa - sb);
            if (p2 < 0.0f) return false;
            const float tmp = std::atan2(cb - ca, d + sa - sb);
            out = {mod2pi(-alpha + tmp), std::sqrt(p2), mod2pi(beta - tmp)};
            return true;
        }
        case DubinsWord::RSR: {
            const float p2 = 2.0f + d * d - 2.0f * cab + 2.0f * d * (sb - sa);
            if (p2 < 0.0f) return false;
            const float tmp = std::atan2(ca - cb, d - sa + sb);
            out = {mod2pi(alpha - tmp), std::sqrt(p2), mod2pi(-beta + tmp)};
            return true;
        }
        case DubinsWord::LSR: {
            const float p2 = -2.0f + d * d + 2.0f * cab + 2.0f * d * (sa + sb);
            if (p2 < 0.0f) return false;
            const float p = std::sqrt(p2);
            const float tmp = std::atan2(-ca - cb, d + sa + sb) - std::atan2(-2.0f, p);
            out = {mod2pi(-alpha + tmp), p, mod2pi(-mod2pi(beta) + tmp)};
            return true;
        }
        case DubinsWord::RSL: {
            const float p2 = d * d - 2.0f + 2.0f * cab - 2.0f * d * (sa + sb);
            if (p2 < 0.0f) return false;
            const float p = std::sqrt(p2);
            const float tmp = std::atan2(ca + cb, d - sa - sb) - std::atan2(2.0f, p);
            out = {mod2pi(alpha - tmp), p, mod2pi(beta - tmp)};
            return true;
        }
        case DubinsWord::RLR: {
            const float t = (6.0f - d * d + 2.0f * cab + 2.0f * d * (sa - sb)) / 8.0f;
            if (std::fabs(t) > 1.0f) return false;
            const float p = mod2pi(6.28318530717958647692f - std::acos(t));
            const float tt = mod2pi(alpha - std::atan2(ca - cb, d - sa + sb) + p / 2.0f);
            const float q = mod2pi(alpha - beta - tt + p);
            out = {tt, p, q};
            return true;
        }
        case DubinsWord::LRL: {
            const float t = (6.0f - d * d + 2.0f * cab + 2.0f * d * (-sa + sb)) / 8.0f;
            if (std::fabs(t) > 1.0f) return false;
            const float p = mod2pi(6.28318530717958647692f - std::acos(t));
            const float tt = mod2pi(-alpha - std::atan2(ca - cb, d + sa - sb) + p / 2.0f);
            const float q = mod2pi(mod2pi(beta) - alpha - tt + p);
            out = {tt, p, q};
            return true;
        }
    }
    return false;
}
} // namespace dubins_detail

// Compute a specific Dubins word between two poses at the given turning radius. `ok` is false when that word
// has no solution for this geometry. Segment lengths are stored normalised (radius = 1).
inline DubinsPath dubinsComputeWord(const Pose2& start, const Pose2& goal, float radius, DubinsWord w) {
    using namespace dubins_detail;
    DubinsPath path;
    path.radius = radius;
    path.start = start;
    path.word = w;
    path.seg = wordSegs(w);
    const vec2 delta = goal.pos - start.pos;
    const float D = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const float d = D / radius;
    const float theta = mod2pi(std::atan2(delta.y, delta.x));
    const float alpha = mod2pi(start.heading - theta);
    const float beta = mod2pi(goal.heading - theta);
    std::array<float, 3> seg{};
    if (!solveWord(w, alpha, beta, d, seg)) {
        return path; // ok stays false
    }
    path.segLen = seg;
    path.length = (seg[0] + seg[1] + seg[2]) * radius;
    path.ok = true;
    return path;
}

// The shortest Dubins path between two poses. `ok` is false only in pathological cases where no word solves
// (should not happen for a positive radius). Ties break by word order LSL, LSR, RSL, RSR, RLR, LRL.
inline DubinsPath dubinsShortestPath(const Pose2& start, const Pose2& goal, float radius) {
    const DubinsWord all[6] = {DubinsWord::LSL, DubinsWord::LSR, DubinsWord::RSL,
                               DubinsWord::RSR, DubinsWord::RLR, DubinsWord::LRL};
    DubinsPath best;
    best.length = 1e30f;
    for (DubinsWord w : all) {
        const DubinsPath c = dubinsComputeWord(start, goal, radius, w);
        if (c.ok && c.length < best.length) {
            best = c;
        }
    }
    if (best.length >= 1e30f) {
        best.ok = false;
        best.length = 0.0f;
    }
    return best;
}

// Sample the pose at arc-length `s` (world units, 0..length) along a path. Following the path to s == length
// lands on the goal pose. Integrates the segment motions in the normalised frame, then scales by the radius.
inline Pose2 dubinsSample(const DubinsPath& path, float s) {
    float tn = s / path.radius; // normalised arc length remaining to walk
    // Work in a normalised frame anchored at the origin with the start heading; scale/translate at the end.
    float x = 0.0f, y = 0.0f, h = path.start.heading;
    for (int i = 0; i < 3; ++i) {
        const float segLen = path.segLen[static_cast<std::size_t>(i)];
        const float step = tn < segLen ? tn : segLen;
        if (step > 0.0f) {
            switch (path.seg[static_cast<std::size_t>(i)]) {
                case DubinsSeg::Left:
                    x += std::sin(h + step) - std::sin(h);
                    y += -std::cos(h + step) + std::cos(h);
                    h += step;
                    break;
                case DubinsSeg::Right:
                    x += -std::sin(h - step) + std::sin(h);
                    y += std::cos(h - step) - std::cos(h);
                    h -= step;
                    break;
                case DubinsSeg::Straight:
                    x += std::cos(h) * step;
                    y += std::sin(h) * step;
                    break;
            }
        }
        tn -= step;
        if (tn <= 0.0f) {
            break;
        }
    }
    Pose2 out;
    out.pos = vec2(x * path.radius + path.start.pos.x, y * path.radius + path.start.pos.y);
    out.heading = dubins_detail::mod2pi(h);
    return out;
}

} // namespace maz::math
