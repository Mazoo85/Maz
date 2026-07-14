#pragma once

// Sphere primitive plus sphere/ray-sphere intersection helpers, complementing
// Geometry.hpp's Aabb/Ray/Plane tests: sphere-sphere and sphere-AABB overlap
// (squared-distance / closest-point, no sqrt on the boolean paths), sphere-plane
// straddle, closest-point-on-AABB, and ray-sphere (earliest t>=0, ray origin may
// be inside). Also ray-triangle (Moeller-Trumbore, earliest t>=0, optional
// backface cull) — the basis for mesh picking / raycasting. The Godot
// collision/broadphase analog; composes iter2-era Geometry.
//
// Conventions:
//   - Ray: dir need NOT be normalized. Intersection parameters (t) are expressed
//     in dir-length units (matching Geometry's Ray/rayAabb), i.e. the hit point is
//     origin + t*dir.
//   - Overlap tests are inclusive: touching == overlapping.
//   - Ray-triangle barycentrics (u,v): u is the weight toward v1, v toward v2, and
//     1-u-v toward v0, so the hit point is v0 + u*(v1-v0) + v*(v2-v0).
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

// Ray vs triangle (v0,v1,v2), Moeller-Trumbore. Returns the earliest t >= 0 hit; writes
// t plus barycentric coords (u,v) of the hit — the point is v0 + u*(v1-v0) + v*(v2-v0)
// (u is the weight toward v1, v toward v2, and 1-u-v toward v0). dir need NOT be
// normalized (t is in dir-length units, matching Ray/rayAabb/raySphere). When
// cullBackface is true, a hit on the triangle's back face (ray hitting the side
// opposite the winding normal) is rejected. Only writes outputs when returning true.
inline bool rayTriangle(const Ray& r, vec3 v0, vec3 v1, vec3 v2,
                        float& tOut, float& uOut, float& vOut,
                        bool cullBackface = false) {
    const float eps = 1e-6f;
    const vec3 edge1 = v1 - v0;
    const vec3 edge2 = v2 - v0;
    const vec3 pvec = cross(r.dir, edge2);
    const float det = dot(edge1, pvec);
    if (cullBackface) {
        if (det < eps) return false;            // back-facing or parallel
    } else {
        if (det > -eps && det < eps) return false; // parallel to the triangle plane
    }
    const float invDet = 1.0f / det;
    const vec3 tvec = r.origin - v0;
    const float u = dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;
    const vec3 qvec = cross(tvec, edge1);
    const float v = dot(r.dir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float t = dot(edge2, qvec) * invDet;
    if (t < 0.0f) return false;                 // intersection is behind the ray origin
    tOut = t; uOut = u; vOut = v;
    return true;
}

// Convenience overload without the barycentric out-params (delegates, discarding u,v).
inline bool rayTriangle(const Ray& r, vec3 v0, vec3 v1, vec3 v2, float& tOut, bool cullBackface = false) {
    float u = 0.0f, v = 0.0f;
    return rayTriangle(r, v0, v1, v2, tOut, u, v, cullBackface);
}

} // namespace maz::math
