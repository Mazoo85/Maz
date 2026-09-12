#pragma once

#include "maz/math/Geometry3D.hpp" // Obb, vec3
#include "maz/math/VectorOps.hpp"  // dot

#include <cmath>

// maz::math closest point on / distance to an oriented bounding box (OBB) — the query the engine's Obb was
// missing. Obb already answers "does it contain this point?" and "do two boxes overlap?" (SAT), but not
// "what is the nearest point on this box, and how far is it?" — the workhorse for sphere-vs-box collision,
// character-vs-crate resolution, editor picking, and proximity/trigger tests against a rotated box. Works by
// dropping the point into the box's local frame (projection onto its axes), clamping per-axis, and mapping
// back. Also provides the EXACT oriented-box signed distance field (negative inside), which the cheap
// "gradient-scaled implicit" only approximates. Godot exposes no OBB closest-point helper. Header-only,
// std-only, deterministic.
namespace maz::math {

// The closest point of the SOLID box to `p` (returns `p` itself when it is inside the box).
inline vec3 closestPointOnObb(const Obb& box, const vec3& p) {
    const vec3 d = p - box.center;
    vec3 q = box.center;
    for (int i = 0; i < 3; ++i) {
        const vec3 ax = box.axis(i);
        float dist = dot(d, ax);
        const float h = box.half[i];
        dist = dist < -h ? -h : (dist > h ? h : dist); // clamp to the slab
        q = q + ax * dist;
    }
    return q;
}

// Squared distance from `p` to the box (0 when inside). Cheaper than distanceToObb when only comparing.
inline float sqDistanceToObb(const Obb& box, const vec3& p) {
    const vec3 d = p - box.center;
    float sq = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float dist = dot(d, box.axis(i));
        const float h = box.half[i];
        const float excess = dist < -h ? (dist + h) : (dist > h ? (dist - h) : 0.0f);
        sq += excess * excess;
    }
    return sq;
}

// Distance from `p` to the box (0 when inside).
inline float distanceToObb(const Obb& box, const vec3& p) { return std::sqrt(sqDistanceToObb(box, p)); }

// The EXACT signed distance to the box: negative strictly inside (the distance to the nearest face), positive
// outside, ~0 on the surface. Standard box-SDF evaluated in the box's local frame.
inline float signedDistanceObb(const Obb& box, const vec3& p) {
    const vec3 d = p - box.center;
    // Local excess per axis: |local coord| - half.
    float qx = std::fabs(dot(d, box.axis(0))) - box.half.x;
    float qy = std::fabs(dot(d, box.axis(1))) - box.half.y;
    float qz = std::fabs(dot(d, box.axis(2))) - box.half.z;
    const float ox = qx > 0.0f ? qx : 0.0f;
    const float oy = qy > 0.0f ? qy : 0.0f;
    const float oz = qz > 0.0f ? qz : 0.0f;
    const float outside = std::sqrt(ox * ox + oy * oy + oz * oz);
    const float inside = std::fmin(std::fmax(qx, std::fmax(qy, qz)), 0.0f);
    return outside + inside;
}

// Does the box overlap the sphere of the given center and radius? (Distance from the sphere centre to the
// box is within the radius.)
inline bool obbIntersectsSphere(const Obb& box, const vec3& sphereCenter, float radius) {
    return sqDistanceToObb(box, sphereCenter) <= radius * radius;
}

} // namespace maz::math
