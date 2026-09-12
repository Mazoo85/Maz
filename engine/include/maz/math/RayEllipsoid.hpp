#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>

// maz::math ray vs axis-aligned ellipsoid — the hitscan / picking test against an ellipsoid with independent
// per-axis radii, returning the hit distance, the world hit point AND the correctly-scaled surface normal.
// A ray-vs-sphere only handles a uniform radius; real colliders and bounding volumes are often squashed or
// stretched (a capsule cap, an egg, a flattened blast radius, a stretched planet), which is exactly an
// ellipsoid. The trick is to warp space so the ellipsoid becomes a unit sphere (divide by the radii), solve
// the sphere quadratic there, then map the hit back — but the normal must be taken from the implicit
// gradient (p-c)/radii^2, NOT the naive warped direction, or it comes out wrong on non-uniform radii. Godot
// exposes no such helper. Pure vec3 math, deterministic, header-only.
namespace maz::math {

struct EllipsoidHit {
    bool hit = false;
    float t = 0.0f;    // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f};  // world hit point
    vec3 normal{0.0f}; // unit outward surface normal at the hit
};

// Intersect the ray `from + t*dir` (t >= 0) with the axis-aligned ellipsoid centred at `center` with the
// positive per-axis radii `radii` (semi-axes along x/y/z). `dir` need not be normalised; `t` is measured in
// units of |dir|. Returns the nearest forward surface hit, or {hit=false}.
inline EllipsoidHit rayIntersectsEllipsoid(const vec3& from, const vec3& dir, const vec3& center,
                                           const vec3& radii) {
    EllipsoidHit best;
    const vec3 inv(1.0f / radii.x, 1.0f / radii.y, 1.0f / radii.z);
    // Warp into unit-sphere space (component-wise divide by the radii).
    const vec3 o((from.x - center.x) * inv.x, (from.y - center.y) * inv.y, (from.z - center.z) * inv.z);
    const vec3 d(dir.x * inv.x, dir.y * inv.y, dir.z * inv.z);

    const float a = dot(d, d);
    if (a < 1e-20f) {
        return best; // degenerate direction
    }
    const float b = 2.0f * dot(o, d);
    const float c = dot(o, o) - 1.0f;
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) {
        return best;
    }
    const float sq = std::sqrt(disc);
    const float t0 = (-b - sq) / (2.0f * a);
    const float t1 = (-b + sq) / (2.0f * a);
    // Nearest non-negative root (t0 <= t1). If t0 < 0 the origin is inside/behind; take t1.
    float t = t0;
    if (t < 0.0f) {
        t = t1;
    }
    if (t < 0.0f) {
        return best;
    }

    best.hit = true;
    best.t = t;
    best.point = from + dir * t;
    // Outward normal from the implicit gradient of sum(((p-c)/radii)^2): (p-c)/radii^2, normalised.
    const vec3 rel(best.point.x - center.x, best.point.y - center.y, best.point.z - center.z);
    vec3 n(rel.x * inv.x * inv.x, rel.y * inv.y * inv.y, rel.z * inv.z * inv.z);
    const float nl = std::sqrt(dot(n, n));
    best.normal = nl > 1e-20f ? n * (1.0f / nl) : vec3(0.0f, 0.0f, 1.0f);
    return best;
}

} // namespace maz::math
