#pragma once

#include "maz/math/Geometry3D.hpp"     // math::closestPointOnTriangle
#include "maz/math/Math.hpp"           // vec3, dot, cross
#include "maz/math/SegmentDistance.hpp" // math::closestBetweenSegments

#include <cmath>

// maz::math capsule-vs-triangle — the workhorse narrowphase test behind a 3D CHARACTER CONTROLLER: does a
// capsule (a segment `p0..p1` swept with radius `r` — the standard player/enemy body shape) touch a triangle
// of the level mesh? The engine already had the pieces for point↔triangle (`closestPointOnTriangle`),
// segment↔segment (`closestBetweenSegments`, and `capsulesOverlap` on top of it), ray↔capsule, and
// sphere/box/OBB overlaps — but not the capsule↔triangle pair that walking a capsule over arbitrary mesh
// geometry needs. This computes the exact minimum distance from the capsule's axis segment to the (filled)
// triangle and compares it to the radius. The hard part is the segment-vs-triangle distance: it is 0 when the
// segment pierces the triangle (handled by an explicit segment/triangle intersection test), otherwise the
// minimum over the two segment endpoints projected onto the triangle and the segment against each of the
// triangle's three edges. Pure vec3 math, header-only, deterministic — exactly unit-testable against
// hand-built configurations. Godot leaves capsule-vs-mesh to its physics server; this is the CPU primitive.
namespace maz::math {

namespace detail {

// Does the segment p0..p1 pierce the (filled) triangle a,b,c? Möller–Trumbore adapted to a finite segment:
// solve for the ray parameter t and require it to land within [0,1] and inside the triangle (u,v>=0, u+v<=1).
inline bool segmentPiercesTriangle(const vec3& p0, const vec3& p1, const vec3& a, const vec3& b,
                                   const vec3& c) {
    const vec3 dir = p1 - p0;
    const vec3 e1 = b - a, e2 = c - a;
    const vec3 pv = cross(dir, e2);
    const float det = dot(e1, pv);
    if (std::fabs(det) < 1e-12f) {
        return false; // segment parallel to triangle plane — no clean pierce (grazing handled by distance path)
    }
    const float inv = 1.0f / det;
    const vec3 tv = p0 - a;
    const float u = dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const vec3 qv = cross(tv, e1);
    const float v = dot(dir, qv) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float t = dot(e2, qv) * inv;
    return t >= 0.0f && t <= 1.0f;
}

} // namespace detail

// Squared minimum distance between the segment p0..p1 and the filled triangle a,b,c. 0 when they touch/cross.
inline float squaredDistanceSegmentTriangle(const vec3& p0, const vec3& p1, const vec3& a, const vec3& b,
                                            const vec3& c) {
    if (detail::segmentPiercesTriangle(p0, p1, a, b, c)) {
        return 0.0f;
    }
    auto d2 = [](const vec3& u, const vec3& w) {
        const vec3 e = u - w;
        return dot(e, e);
    };
    // Segment endpoints projected onto the triangle (captures endpoint-over-interior/edge/vertex, and the
    // perpendicular case where the segment runs parallel above the interior).
    float best = d2(p0, closestPointOnTriangle(p0, a, b, c));
    best = std::fmin(best, d2(p1, closestPointOnTriangle(p1, a, b, c)));
    // Segment against each triangle edge (captures the segment interior crossing over an edge in projection).
    const vec3 tri[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        const float e = closestBetweenSegments(p0, p1, tri[i], tri[(i + 1) % 3]).distance;
        best = std::fmin(best, e * e);
    }
    return best;
}

// Does a capsule (segment p0..p1, radius r) overlap the triangle a,b,c?
inline bool capsuleIntersectsTriangle(const vec3& p0, const vec3& p1, float r, const vec3& a, const vec3& b,
                                      const vec3& c) {
    const float rr = (r < 0.0f ? 0.0f : r);
    return squaredDistanceSegmentTriangle(p0, p1, a, b, c) <= rr * rr;
}

} // namespace maz::math
