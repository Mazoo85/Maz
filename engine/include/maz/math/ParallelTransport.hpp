#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math rotation-minimizing (parallel-transport) frames — a smoothly twisting coordinate frame that
// rides along a 3D path. To sweep a cross-section down a curve — a tube, rope, cable, road, vine, ribbon
// trail, or a camera rail — you need, at every point, a consistent "up"/"side" pair perpendicular to the
// direction of travel. The textbook Frenet frame (built from the curve's curvature) works until the curve
// straightens or inflects, where it suddenly flips 180 degrees and the swept mesh visibly kinks. A
// rotation-minimizing frame instead carries the previous frame forward with the LEAST possible twist, so
// the tube never spins or snaps. This uses Wang et al.'s double-reflection method, which is exact and
// stable. Godot has no such utility (its CSGPolygon path-extrude twists on inflections). Header-only,
// std-only, deterministic.
namespace maz::math {

struct Frame {
    vec3 tangent{0.0f, 0.0f, 1.0f};
    vec3 normal{0.0f, 1.0f, 0.0f};
    vec3 binormal{1.0f, 0.0f, 0.0f};
};

namespace detail {
inline vec3 ptNormalize(const vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return len > 1e-12f ? v / len : v;
}
inline float ptDot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 ptCross(const vec3& a, const vec3& b) {
    return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
} // namespace detail

// Build one rotation-minimizing frame per input point. `upHint` seeds the initial normal (any vector not
// parallel to the first tangent); the rest are transported. Fewer than two points yields an empty result.
inline std::vector<Frame> parallelTransportFrames(const std::vector<vec3>& points,
                                                  vec3 upHint = vec3(0.0f, 1.0f, 0.0f)) {
    std::vector<Frame> frames;
    const std::size_t n = points.size();
    if (n < 2) {
        return frames;
    }
    // Per-vertex tangents (central differences interior, one-sided at the ends).
    std::vector<vec3> T(n);
    T[0] = detail::ptNormalize(points[1] - points[0]);
    T[n - 1] = detail::ptNormalize(points[n - 1] - points[n - 2]);
    for (std::size_t i = 1; i + 1 < n; ++i) {
        T[i] = detail::ptNormalize(points[i + 1] - points[i - 1]);
    }
    // Seed the first normal: project the hint off the tangent; fall back if degenerate.
    vec3 n0 = upHint - T[0] * detail::ptDot(upHint, T[0]);
    if (detail::ptDot(n0, n0) < 1e-10f) {
        vec3 alt = std::fabs(T[0].x) < 0.9f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 1.0f, 0.0f);
        n0 = alt - T[0] * detail::ptDot(alt, T[0]);
    }
    n0 = detail::ptNormalize(n0);
    frames.resize(n);
    frames[0].tangent = T[0];
    frames[0].normal = n0;
    frames[0].binormal = detail::ptNormalize(detail::ptCross(T[0], n0));
    // Double-reflection transport (Wang 2008): reflect the reference frame across the segment, then across
    // the tangent bisector, giving the minimally-rotated normal at the next sample.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const vec3 v1 = points[i + 1] - points[i];
        const float c1 = detail::ptDot(v1, v1);
        if (c1 < 1e-12f) {
            frames[i + 1] = frames[i];
            frames[i + 1].tangent = T[i + 1];
            continue;
        }
        const vec3 rL = frames[i].normal - v1 * (2.0f / c1) * detail::ptDot(v1, frames[i].normal);
        const vec3 tL = T[i] - v1 * (2.0f / c1) * detail::ptDot(v1, T[i]);
        const vec3 v2 = T[i + 1] - tL;
        const float c2 = detail::ptDot(v2, v2);
        vec3 rNext = rL;
        if (c2 > 1e-12f) {
            rNext = rL - v2 * (2.0f / c2) * detail::ptDot(v2, rL);
        }
        rNext = detail::ptNormalize(rNext);
        frames[i + 1].tangent = T[i + 1];
        frames[i + 1].normal = rNext;
        frames[i + 1].binormal = detail::ptNormalize(detail::ptCross(T[i + 1], rNext));
    }
    return frames;
}

} // namespace maz::math
