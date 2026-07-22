#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>

// maz::math closest points between two 3D line segments — the geometry primitive at the heart of
// capsule-vs-capsule collision (a capsule is a segment + radius, so two capsules overlap exactly when
// the closest distance between their spine segments is < the sum of radii), and generally the answer to
// "how far apart are these two edges, and where?" The engine already has point-vs-segment (2D) and
// point-vs-triangle (3D) closest-point queries; this fills in segment-vs-segment. It uses Ericson's
// robust algorithm (Real-Time Collision Detection): parameterize each segment by s,t in [0,1], solve
// the unconstrained minimum, then clamp into the valid square, handling parallel and degenerate
// (zero-length) segments without dividing by zero. Pure vec3 math — exactly unit-testable against
// hand-computed configurations.
namespace maz::math {

struct SegmentClosest {
    vec3 pointA{0.0f, 0.0f, 0.0f}; // closest point on segment A
    vec3 pointB{0.0f, 0.0f, 0.0f}; // closest point on segment B
    float s = 0.0f;                // parameter along A (p1 -> q1)
    float t = 0.0f;                // parameter along B (p2 -> q2)
    float distance = 0.0f;         // |pointA - pointB|
};

namespace detail {
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
} // namespace detail

// Closest points between segment A (p1->q1) and segment B (p2->q2).
inline SegmentClosest closestBetweenSegments(const vec3& p1, const vec3& q1, const vec3& p2,
                                             const vec3& q2) {
    const vec3 d1 = q1 - p1; // direction of segment A
    const vec3 d2 = q2 - p2; // direction of segment B
    const vec3 r = p1 - p2;
    const float a = detail::dot3(d1, d1); // squared length of A
    const float e = detail::dot3(d2, d2); // squared length of B
    const float f = detail::dot3(d2, r);
    const float eps = 1e-12f;

    float s = 0.0f;
    float t = 0.0f;
    if (a <= eps && e <= eps) {
        // Both segments degenerate to points.
        s = 0.0f;
        t = 0.0f;
    } else if (a <= eps) {
        // First segment degenerate.
        s = 0.0f;
        t = detail::clamp01(f / e);
    } else {
        const float c = detail::dot3(d1, r);
        if (e <= eps) {
            // Second segment degenerate.
            t = 0.0f;
            s = detail::clamp01(-c / a);
        } else {
            const float b = detail::dot3(d1, d2);
            const float denom = a * e - b * b; // always >= 0
            if (denom > eps) {
                s = detail::clamp01((b * f - c * e) / denom);
            } else {
                s = 0.0f; // parallel: pick an arbitrary point on A, optimize t
            }
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = detail::clamp01(-c / a);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = detail::clamp01((b - c) / a);
            }
        }
    }

    SegmentClosest out;
    out.s = s;
    out.t = t;
    out.pointA = p1 + d1 * s;
    out.pointB = p2 + d2 * t;
    const vec3 diff = out.pointA - out.pointB;
    out.distance = std::sqrt(detail::dot3(diff, diff));
    return out;
}

// Convenience: do two capsules (segment + radius) overlap?
inline bool capsulesOverlap(const vec3& a0, const vec3& a1, float ra, const vec3& b0, const vec3& b1,
                            float rb) {
    return closestBetweenSegments(a0, a1, b0, b1).distance <= (ra + rb);
}

} // namespace maz::math
