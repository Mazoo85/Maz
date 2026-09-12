#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>

// maz::math ray vs flat planar shapes — disk, annulus (ring) and oriented rectangle, each an arbitrarily
// placed and oriented plane patch, returning the hit distance, world point and a normal that faces the ray.
// These are the pick/hit tests you need for flat things a game puts in 3D space: circular platforms, jump
// pads and portals (disk), ring pickups / hula-hoops (annulus), floating UI panels, billboards, doors and
// signboards (oriented rectangle). Godot has no direct ray-vs-disk / ring / oriented-quad helper. Each test
// intersects the ray with the shape's plane, then does a cheap in-plane containment check. The returned
// normal always points back toward the ray so lighting/decals read correctly from either side. Pure vec3
// math, deterministic, header-only.
namespace maz::math {

struct PlanarHit {
    bool hit = false;
    float t = 0.0f;    // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f};  // world hit point
    vec3 normal{0.0f}; // unit normal, oriented to face the incoming ray
};

namespace detail {
// Intersect the ray with the plane through `center` with unit `normal`; fills t and point, returns false if
// the ray is parallel or the hit is behind the origin.
inline bool rayPlane(const vec3& from, const vec3& dir, const vec3& center, const vec3& normal, float& t,
                     vec3& point) {
    const float denom = dot(dir, normal);
    if (std::fabs(denom) < 1e-9f) {
        return false; // parallel to the plane
    }
    t = dot(center - from, normal) / denom;
    if (t < 0.0f) {
        return false;
    }
    point = from + dir * t;
    return true;
}
inline vec3 faceRay(const vec3& normal, const vec3& dir) {
    return dot(normal, dir) > 0.0f ? vec3(-normal.x, -normal.y, -normal.z) : normal;
}
} // namespace detail

// Ray vs a filled disk of `radius` centred at `center` with unit face `normal`.
inline PlanarHit rayIntersectsDisk(const vec3& from, const vec3& dir, const vec3& center, const vec3& normal,
                                   float radius) {
    PlanarHit h;
    float t;
    vec3 p;
    if (!detail::rayPlane(from, dir, center, normal, t, p)) {
        return h;
    }
    const vec3 r = p - center;
    if (dot(r, r) <= radius * radius) {
        h.hit = true;
        h.t = t;
        h.point = p;
        h.normal = detail::faceRay(normal, dir);
    }
    return h;
}

// Ray vs an annulus (flat ring) with inner and outer radii, centred at `center` with unit face `normal`.
inline PlanarHit rayIntersectsAnnulus(const vec3& from, const vec3& dir, const vec3& center,
                                      const vec3& normal, float innerRadius, float outerRadius) {
    PlanarHit h;
    float t;
    vec3 p;
    if (!detail::rayPlane(from, dir, center, normal, t, p)) {
        return h;
    }
    const vec3 r = p - center;
    const float d2 = dot(r, r);
    if (d2 >= innerRadius * innerRadius && d2 <= outerRadius * outerRadius) {
        h.hit = true;
        h.t = t;
        h.point = p;
        h.normal = detail::faceRay(normal, dir);
    }
    return h;
}

// Ray vs an oriented rectangle: centre, two UNIT in-plane axes `uAxis`/`vAxis`, and half-extents along each.
// The rectangle's face normal is uAxis × vAxis.
inline PlanarHit rayIntersectsRect(const vec3& from, const vec3& dir, const vec3& center, const vec3& uAxis,
                                   const vec3& vAxis, float halfU, float halfV) {
    PlanarHit h;
    const vec3 n(uAxis.y * vAxis.z - uAxis.z * vAxis.y, uAxis.z * vAxis.x - uAxis.x * vAxis.z,
                 uAxis.x * vAxis.y - uAxis.y * vAxis.x);
    const float nl = std::sqrt(dot(n, n));
    if (nl < 1e-9f) {
        return h; // degenerate axes
    }
    const vec3 normal = n * (1.0f / nl);
    float t;
    vec3 p;
    if (!detail::rayPlane(from, dir, center, normal, t, p)) {
        return h;
    }
    const vec3 r = p - center;
    const float du = dot(r, uAxis);
    const float dv = dot(r, vAxis);
    if (std::fabs(du) <= halfU && std::fabs(dv) <= halfV) {
        h.hit = true;
        h.t = t;
        h.point = p;
        h.normal = detail::faceRay(normal, dir);
    }
    return h;
}

} // namespace maz::math
