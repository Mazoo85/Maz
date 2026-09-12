#pragma once

#include "maz/math/Geometry3D.hpp" // Obb, vec3, dot

#include <cmath>

// maz::math closest point on an oriented bounding box (OBB) — given a point and an arbitrarily-rotated box,
// return the point on (or in) the box nearest to it, and the distance. The engine's Obb (Geometry3D.hpp)
// already does contains / box-vs-box SAT / AABB bounds, but not this proximity query, which is what you need
// for sphere-vs-OBB collision (overlap iff distance ≤ radius, with the closest point as the contact),
// snapping a probe/agent to the outside of a crate, distance-based culling and trigger volumes, and nearest-
// surface queries in an editor. It works by expressing the point in the box's local frame, clamping each
// local coordinate to the box's half-extents, and mapping back to world — so a point inside the box returns
// itself (distance 0) and a point outside returns the nearest surface point. Godot exposes no such helper.
// Pure vec3 math, deterministic, header-only.
namespace maz::math {

// The point on or inside `box` closest to `p`. For a point inside the box this is `p` itself.
inline vec3 closestPointOnObb(const vec3& p, const Obb& box) {
    const vec3 d = p - box.center;
    vec3 result = box.center;
    for (int i = 0; i < 3; ++i) {
        const vec3 a = box.axis(i);
        float dist = dot(d, a);
        if (dist > box.half[i]) {
            dist = box.half[i];
        } else if (dist < -box.half[i]) {
            dist = -box.half[i];
        }
        result = result + a * dist;
    }
    return result;
}

// Distance from `p` to `box` (0 when `p` is inside the box).
inline float distanceToObb(const vec3& p, const Obb& box) {
    const vec3 c = closestPointOnObb(p, box);
    const vec3 v = p - c;
    return std::sqrt(dot(v, v));
}

// True when the sphere (center, radius) overlaps `box`. Writes the closest point on the box to `contact`.
inline bool sphereIntersectsObb(const vec3& center, float radius, const Obb& box, vec3& contact) {
    contact = closestPointOnObb(center, box);
    const vec3 v = center - contact;
    return dot(v, v) <= radius * radius;
}

} // namespace maz::math
