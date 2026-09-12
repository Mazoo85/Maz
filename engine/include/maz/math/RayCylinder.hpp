#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot, length, lengthSquared

#include <cmath>

// maz::math ray vs finite capped cylinder — the hitscan / picking test against a cylinder with an ARBITRARY
// axis and position, returning the hit distance, the world hit point AND the surface normal. The engine's
// Geometry3D.segmentIntersectsCylinder only handles a segment against an origin-centred, Y-aligned cylinder
// and returns just a point; this is the general ray query you need to shoot at pillars, tree trunks, barrels,
// pipes and cylindrical colliders, or to pick them in an editor. It tests the curved side (a quadratic on
// the ray projected perpendicular to the axis, clamped to the cylinder's length) and both end caps, and
// returns the nearest forward hit with the correct outward normal (radial on the side, ±axis on a cap).
// Godot exposes no such helper. Pure vec3 math, deterministic, header-only.
namespace maz::math {

struct CylinderHit {
    bool hit = false;
    float t = 0.0f;          // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f};        // world hit point
    vec3 normal{0.0f};       // unit outward surface normal at the hit
};

// Intersect the ray `from + t*dir` (t >= 0) with the finite cylinder whose axis runs from `base` for
// `height` along the unit vector `axis`, with the given `radius`. `dir` and `axis` need not be normalised
// except that `axis` is treated as a unit direction. Returns the nearest forward surface hit, or {hit=false}.
inline CylinderHit rayIntersectsCylinder(const vec3& from, const vec3& dir, const vec3& base,
                                         const vec3& axis, float radius, float height) {
    CylinderHit best;
    best.t = 1e30f;
    const vec3 ax = axis; // assumed unit length
    const float R2 = radius * radius;

    auto consider = [&](float t, const vec3& p, const vec3& n) {
        if (t >= 0.0f && t < best.t) {
            best.hit = true;
            best.t = t;
            best.point = p;
            best.normal = n;
        }
    };

    // --- Curved side: solve |perp(o + d t)|^2 = R^2, where perp removes the axial component. ---
    const vec3 o = from - base;
    const float dA = dot(dir, ax);
    const float oA = dot(o, ax);
    const vec3 dPerp = dir - ax * dA;   // ray direction perpendicular to axis
    const vec3 oPerp = o - ax * oA;     // ray origin (rel. base) perpendicular to axis
    const float a = dot(dPerp, dPerp);
    const float b = 2.0f * dot(dPerp, oPerp);
    const float c = dot(oPerp, oPerp) - R2;
    if (a > 1e-12f) {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float sq = std::sqrt(disc);
            const float roots[2] = {(-b - sq) / (2.0f * a), (-b + sq) / (2.0f * a)};
            for (float t : roots) {
                if (t < 0.0f) {
                    continue;
                }
                const float h = oA + dA * t; // axial coordinate of the hit
                if (h >= 0.0f && h <= height) {
                    const vec3 p = from + dir * t;
                    const vec3 radial = (p - base) - ax * h;
                    const float rl = std::sqrt(dot(radial, radial));
                    const vec3 n = rl > 1e-9f ? radial * (1.0f / rl) : ax;
                    consider(t, p, n);
                }
            }
        }
    }

    // --- End caps: two disks at h = 0 and h = height, normal ±axis. ---
    if (std::fabs(dA) > 1e-12f) {
        const float caps[2] = {0.0f, height};
        for (int i = 0; i < 2; ++i) {
            const float t = (caps[i] - oA) / dA; // plane dot(p-base, ax) = caps[i]
            if (t < 0.0f) {
                continue;
            }
            const vec3 p = from + dir * t;
            const vec3 radial = (p - base) - ax * caps[i];
            if (dot(radial, radial) <= R2) {
                const vec3 n = (i == 0) ? vec3(-ax.x, -ax.y, -ax.z) : ax;
                consider(t, p, n);
            }
        }
    }

    if (!best.hit) {
        best.t = 0.0f;
    }
    return best;
}

} // namespace maz::math
