#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cstddef>
#include <vector>

// maz::math uniform cubic B-spline — a smooth curve that APPROXIMATES a list of control points.
//
// The engine already has cubic Bezier (Curve2D) and centripetal Catmull-Rom (CatmullRomSpline). Those
// INTERPOLATE — the curve passes through every control point — which is what you want for waypoints.
// The cubic B-spline is the other classic: it does NOT pass through its control points, it is pulled
// toward them, and in exchange it is C2 continuous (curvature is continuous, not just the tangent, so
// there are no visible kinks in acceleration) and provably stays inside the convex hull of the four
// local control points (it can never overshoot). That combination — smoothest possible motion, no
// overshoot — is why B-splines are the standard for camera dollies, easing rails, and procedural
// geometry, and why they are the foundation NURBS is built on.
//
// Each segment i is a convex-combination blend of four consecutive control points P0..P3 with the
// uniform cubic basis (all weights >= 0 on [0,1], summing to 1 — the partition of unity). Two chain
// forms are provided: OPEN (n>=4 control points give n-3 segments; the curve starts near P1 and ends
// near P[n-2]) and CLOSED (n>=3 control points wrap into n segments forming a seamless C2 loop).
// Pure vec2 math, no per-eval allocation — deterministic and exactly unit-testable (partition of
// unity, the knot-point average (P0+4P1+P2)/6, the central-difference tangent (P2-P0)/2, linear
// precision, and convex-hull containment all hold on the dot).
namespace maz::math {

// One uniform-cubic-B-spline segment: blend P0..P3 at local parameter t in [0,1].
//   B(t) = 1/6 [ (1-t)^3 P0 + (3t^3-6t^2+4) P1 + (-3t^3+3t^2+3t+1) P2 + t^3 P3 ]
inline vec2 bsplinePoint(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float mt = 1.0f - t;
    const float w0 = mt * mt * mt;
    const float w1 = 3.0f * t3 - 6.0f * t2 + 4.0f;
    const float w2 = -3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f;
    const float w3 = t3;
    const float k = 1.0f / 6.0f;
    return vec2(k * (w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x),
                k * (w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y));
}

// Derivative dB/dt of one segment (an unnormalised tangent).
//   B'(t) = 1/6 [ -3(1-t)^2 P0 + (9t^2-12t) P1 + (-9t^2+6t+3) P2 + 3t^2 P3 ]
inline vec2 bsplineTangent(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
    const float t2 = t * t;
    const float mt = 1.0f - t;
    const float w0 = -3.0f * mt * mt;
    const float w1 = 9.0f * t2 - 12.0f * t;
    const float w2 = -9.0f * t2 + 6.0f * t + 3.0f;
    const float w3 = 3.0f * t2;
    const float k = 1.0f / 6.0f;
    return vec2(k * (w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x),
                k * (w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y));
}

// Number of segments in a chain of `n` control points. Open needs >= 4 (n-3 segments); closed wraps
// (n segments for n >= 3). Returns 0 when there are too few points.
inline int bsplineSegmentCount(std::size_t n, bool closed) {
    if (closed) {
        return n >= 3 ? static_cast<int>(n) : 0;
    }
    return n >= 4 ? static_cast<int>(n) - 3 : 0;
}

// Evaluate a whole B-spline chain at global parameter u in [0, segmentCount]. The integer part selects
// the segment, the fraction is the local t. For CLOSED chains the four control points wrap modulo n.
// Returns the origin when there are too few control points.
inline vec2 bsplineEval(const std::vector<vec2>& ctrl, float u, bool closed = false) {
    const int segs = bsplineSegmentCount(ctrl.size(), closed);
    if (segs <= 0) {
        return vec2(0.0f, 0.0f);
    }
    // Clamp u into [0, segs] and split into segment index + local t.
    if (u < 0.0f) u = 0.0f;
    if (u > static_cast<float>(segs)) u = static_cast<float>(segs);
    int seg = static_cast<int>(u);
    if (seg >= segs) seg = segs - 1; // u == segs lands at the very end of the last segment
    const float t = u - static_cast<float>(seg);
    const std::size_t n = ctrl.size();
    if (closed) {
        const vec2& p0 = ctrl[static_cast<std::size_t>(seg) % n];
        const vec2& p1 = ctrl[(static_cast<std::size_t>(seg) + 1) % n];
        const vec2& p2 = ctrl[(static_cast<std::size_t>(seg) + 2) % n];
        const vec2& p3 = ctrl[(static_cast<std::size_t>(seg) + 3) % n];
        return bsplinePoint(p0, p1, p2, p3, t);
    }
    const std::size_t i = static_cast<std::size_t>(seg);
    return bsplinePoint(ctrl[i], ctrl[i + 1], ctrl[i + 2], ctrl[i + 3], t);
}

// Tangent of a whole chain at global parameter u (same conventions as bsplineEval).
inline vec2 bsplineEvalTangent(const std::vector<vec2>& ctrl, float u, bool closed = false) {
    const int segs = bsplineSegmentCount(ctrl.size(), closed);
    if (segs <= 0) {
        return vec2(0.0f, 0.0f);
    }
    if (u < 0.0f) u = 0.0f;
    if (u > static_cast<float>(segs)) u = static_cast<float>(segs);
    int seg = static_cast<int>(u);
    if (seg >= segs) seg = segs - 1;
    const float t = u - static_cast<float>(seg);
    const std::size_t n = ctrl.size();
    if (closed) {
        return bsplineTangent(ctrl[static_cast<std::size_t>(seg) % n],
                              ctrl[(static_cast<std::size_t>(seg) + 1) % n],
                              ctrl[(static_cast<std::size_t>(seg) + 2) % n],
                              ctrl[(static_cast<std::size_t>(seg) + 3) % n], t);
    }
    const std::size_t i = static_cast<std::size_t>(seg);
    return bsplineTangent(ctrl[i], ctrl[i + 1], ctrl[i + 2], ctrl[i + 3], t);
}

} // namespace maz::math
