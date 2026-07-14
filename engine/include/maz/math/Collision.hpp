#pragma once

// Sphere primitive plus sphere/ray-sphere intersection helpers, complementing
// Geometry.hpp's Aabb/Ray/Plane tests: sphere-sphere and sphere-AABB overlap
// (squared-distance / closest-point, no sqrt on the boolean paths), sphere-plane
// straddle, closest-point-on-AABB, and ray-sphere (earliest t>=0, ray origin may
// be inside). The Godot collision/broadphase analog; composes iter2-era Geometry.
//
// Conventions:
//   - Ray: dir need NOT be normalized. Intersection parameters (t) are expressed
//     in dir-length units (matching Geometry's Ray/rayAabb), i.e. the hit point is
//     origin + t*dir.
//   - Overlap tests are inclusive: touching == overlapping.
//   - NOT a full physics solver — contact manifolds, GJK/EPA, and swept tests are
//     future refinements.

#include "maz/math/Geometry.hpp"
#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <cmath>

namespace maz::math {

// Bounding sphere. An inclusive (closed) volume: points on the surface count as
// contained.
struct Sphere {
    vec3 center;
    float radius;

    // Closed/inclusive containment via squared distance (no sqrt).
    bool contains(vec3 p) const {
        const vec3 d = p - center;
        return dot(d, d) <= radius * radius;
    }
};

// Closest point on/in the box to p (per-component clamp into [a.min, a.max]).
inline vec3 closestPointOnAabb(vec3 p, const Aabb& a) {
    return glm::clamp(p, a.min, a.max);
}

// Inclusive sphere-sphere overlap (touching counts). Squared distance vs summed
// radii, no sqrt.
inline bool sphereSphere(const Sphere& s1, const Sphere& s2) {
    const vec3 d = s2.center - s1.center;
    const float r = s1.radius + s2.radius;
    return dot(d, d) <= r * r;
}

// Inclusive sphere-AABB overlap: closest point on the box to the sphere center,
// then compare squared distance to radius squared.
inline bool sphereAabb(const Sphere& s, const Aabb& a) {
    const vec3 q = closestPointOnAabb(s.center, a);
    const vec3 d = s.center - q;
    return dot(d, d) <= s.radius * s.radius;
}

// Sphere touches/straddles the plane iff the center is within radius of it.
inline bool spherePlane(const Sphere& s, const Plane& pl) {
    return std::fabs(pl.signedDistance(s.center)) <= s.radius;
}

// Ray vs sphere. Solves |origin + t*dir - center|^2 = radius^2 and returns the
// earliest non-negative hit. dir need not be normalized (t is in ray-direction
// length units, matching Geometry's Ray/rayAabb convention). A ray originating
// inside the sphere returns the exit t. tOut is written only on a true return.
inline bool raySphere(const Ray& r, const Sphere& s, float& tOut) {
    const vec3 oc = r.origin - s.center;
    const float aq = dot(r.dir, r.dir);
    MAZ_ASSERT(aq > 0.0f, "raySphere: zero-length ray direction");
    const float bq = 2.0f * dot(oc, r.dir);
    const float cq = dot(oc, oc) - s.radius * s.radius;
    const float disc = bq * bq - 4.0f * aq * cq;
    if (disc < 0.0f) {
        return false; // no intersection
    }
    const float sq = std::sqrt(disc);
    float t = (-bq - sq) / (2.0f * aq); // near root (entry)
    if (t < 0.0f) {
        t = (-bq + sq) / (2.0f * aq); // origin inside sphere -> use far root (exit)
    }
    if (t < 0.0f) {
        return false; // sphere entirely behind the ray
    }
    tOut = t;
    return true;
}

} // namespace maz::math
