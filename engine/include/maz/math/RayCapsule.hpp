#pragma once

#include "maz/math/Math.hpp"      // vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>

// maz::math ray vs capsule — the hitscan / picking test against a capsule (a line segment "swept" by a
// sphere of radius r: every point within distance r of the segment a→b), returning the hit distance, the
// world hit point AND the surface normal. The capsule is the workhorse collider for characters, limbs,
// pills and rounded pipes; this is the general ray query you shoot at them or use to pick them in an editor.
// It tests the CYLINDRICAL SIDE (a quadratic on the ray projected perpendicular to the axis, clamped to the
// segment's length) and the two HEMISPHERICAL end caps (the spheres at a and b, each restricted to its own
// hemisphere so the caps meet the side exactly with no double surface), and returns the nearest forward hit
// with the correct outward unit normal — radial on the side, and (p − nearest-endpoint)/r on a cap, which is
// exactly normalize(p − closestPointOnSegment). Godot's ray query lives inside its physics server and needs
// a live body; this is a free-standing vec3 helper. Deterministic, header-only, std-only.
namespace maz::math {

struct CapsuleHit {
    bool hit = false;
    float t = 0.0f;   // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f}; // world hit point
    vec3 normal{0.0f}; // unit outward surface normal at the hit
};

// Intersect the ray `from + t*dir` (t >= 0) with the capsule of radius `radius` whose spine runs from `a`
// to `b`. `dir` need not be normalised (t is then in units of |dir|). A degenerate spine (a == b) is a
// sphere. Returns the nearest forward surface hit, or {hit=false}.
inline CapsuleHit rayIntersectsCapsule(const vec3& from, const vec3& dir, const vec3& a, const vec3& b,
                                       float radius) {
    CapsuleHit best;
    best.t = 1e30f;
    const float R2 = radius * radius;

    auto consider = [&](float t, const vec3& p, const vec3& n) {
        if (t >= 0.0f && t < best.t) {
            best.hit = true;
            best.t = t;
            best.point = p;
            best.normal = n;
        }
    };

    const vec3 ba = b - a;
    const float L2 = dot(ba, ba);

    // --- Cylindrical side: solve |perp(from + dir t - a)|^2 = R^2, then clamp the axial coordinate to
    //     the segment. Skipped for a degenerate (zero-length) spine, which is a pure sphere. ---
    if (L2 > 1e-12f) {
        const vec3 ax = ba * (1.0f / std::sqrt(L2)); // unit spine direction
        const float L = std::sqrt(L2);
        const vec3 o = from - a;
        const float dA = dot(dir, ax);
        const float oA = dot(o, ax);
        const vec3 dPerp = dir - ax * dA; // ray direction perpendicular to the spine
        const vec3 oPerp = o - ax * oA;   // ray origin (rel. a) perpendicular to the spine
        const float qa = dot(dPerp, dPerp);
        const float qb = 2.0f * dot(dPerp, oPerp);
        const float qc = dot(oPerp, oPerp) - R2;
        if (qa > 1e-12f) {
            const float disc = qb * qb - 4.0f * qa * qc;
            if (disc >= 0.0f) {
                const float sq = std::sqrt(disc);
                const float roots[2] = {(-qb - sq) / (2.0f * qa), (-qb + sq) / (2.0f * qa)};
                for (float t : roots) {
                    if (t < 0.0f) {
                        continue;
                    }
                    const float h = oA + dA * t; // axial coordinate of the hit along [0, L]
                    if (h >= 0.0f && h <= L) {
                        const vec3 p = from + dir * t;
                        const vec3 radial = (p - a) - ax * h;
                        const float rl = std::sqrt(dot(radial, radial));
                        const vec3 n = rl > 1e-9f ? radial * (1.0f / rl) : ax;
                        consider(t, p, n);
                    }
                }
            }
        }
    }

    // --- Hemispherical end caps: the spheres at a and b. Each sphere hit is accepted only on its own
    //     side of the segment (axial coord <= 0 for a, >= L for b) so a cap surface is exactly the outer
    //     hemisphere and never overlaps the cylindrical side. For a == b the whole sphere is accepted. ---
    const float axialCut = (L2 > 1e-12f) ? 1.0f : 0.0f; // 0 disables the hemisphere restriction (sphere case)
    auto sphere = [&](const vec3& c, bool isEndB) {
        const vec3 oc = from - c;
        const float qa = dot(dir, dir);
        if (qa <= 1e-20f) {
            return;
        }
        const float qb = 2.0f * dot(oc, dir);
        const float qc = dot(oc, oc) - R2;
        const float disc = qb * qb - 4.0f * qa * qc;
        if (disc < 0.0f) {
            return;
        }
        const float sq = std::sqrt(disc);
        const float roots[2] = {(-qb - sq) / (2.0f * qa), (-qb + sq) / (2.0f * qa)};
        for (float t : roots) {
            if (t < 0.0f) {
                continue;
            }
            const vec3 p = from + dir * t;
            if (axialCut > 0.0f) {
                // Keep only the outward hemisphere: below a (h <= 0) or above b (h >= L).
                const float h = dot(p - a, b - a); // sign of the axial coordinate (scaled by |ba|)
                if (!isEndB && h > 0.0f) {
                    continue;
                }
                if (isEndB && h < L2) {
                    continue;
                }
            }
            const vec3 radial = p - c;
            const float rl = std::sqrt(dot(radial, radial));
            const vec3 n = rl > 1e-9f ? radial * (1.0f / rl) : vec3(0.0f, 1.0f, 0.0f);
            consider(t, p, n);
        }
    };
    sphere(a, false);
    if (L2 > 1e-12f) {
        sphere(b, true);
    }

    if (!best.hit) {
        best.t = 0.0f;
    }
    return best;
}

} // namespace maz::math
