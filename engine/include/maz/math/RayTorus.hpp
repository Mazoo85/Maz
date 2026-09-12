#pragma once

#include "maz/math/Math.hpp"       // vec3
#include "maz/math/VectorOps.hpp"  // dot
#include "maz/math/Polynomial.hpp" // solveQuartic

#include <cmath>

// maz::math ray vs torus (donut) — the hitscan / picking test against a torus with an arbitrary centre and
// axis, returning the hit distance, world point AND outward surface normal. Tori are rings, donuts, tube
// loops, portal rims, tyres, halos and orbit bands; shooting or clicking one has no closed-form quadratic
// answer — it is a genuine QUARTIC in the ray parameter — so most engines (Godot included) simply do not
// offer it. This transforms the ray into the torus's local frame (axis = local z), builds the quartic
//     (|P|^2 + R^2 - r^2)^2 = 4 R^2 (Px^2 + Py^2),
// solves it exactly with math::solveQuartic, and returns the nearest forward hit with the correct normal
// (pointing from the nearest point on the tube's centre circle to the surface point). `R` is the major
// radius (centre to tube centre), `r` the minor radius (tube thickness). Deterministic, header-only.
namespace maz::math {

struct TorusHit {
    bool hit = false;
    float t = 0.0f;    // ray parameter of the hit (from + t*dir), t >= 0
    vec3 point{0.0f};  // world hit point
    vec3 normal{0.0f}; // unit outward surface normal at the hit
};

// Intersect the ray `from + t*dir` (t >= 0, `dir` unit) with the torus centred at `center`, its axis along
// the unit vector `axis`, major radius `R` and minor radius `r` (r < R for a ring torus). Returns the nearest
// forward surface hit, or {hit=false}.
inline TorusHit rayIntersectsTorus(const vec3& from, const vec3& dir, const vec3& center, const vec3& axis,
                                   float R, float r) {
    TorusHit best;

    // Orthonormal local frame: w = axis, (u, v) span the torus plane.
    const vec3 w = axis;
    const vec3 seed = std::fabs(w.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0);
    vec3 u = seed - w * dot(seed, w);
    u = u * (1.0f / std::sqrt(dot(u, u)));
    const vec3 v = vec3(w.y * u.z - w.z * u.y, w.z * u.x - w.x * u.z, w.x * u.y - w.y * u.x);

    // Ray in local coordinates (origin relative to centre).
    const vec3 rel = from - center;
    const vec3 o(dot(rel, u), dot(rel, v), dot(rel, w));
    const vec3 d(dot(dir, u), dot(dir, v), dot(dir, w));

    // Quartic  (|o+td|^2 + R^2 - r^2)^2 - 4 R^2 ((ox+t dx)^2 + (oy+t dy)^2) = 0.
    const float od = dot(o, d);
    const float oo = dot(o, o);
    const float R2 = R * R;
    const float A = dot(d, d);                 // ~1
    const float B = 2.0f * od;
    const float Cc = oo + R2 - r * r;
    const float E = d.x * d.x + d.y * d.y;
    const float F = 2.0f * (o.x * d.x + o.y * d.y);
    const float G = o.x * o.x + o.y * o.y;

    const float c4 = A * A;
    const float c3 = 2.0f * A * B;
    const float c2 = B * B + 2.0f * A * Cc - 4.0f * R2 * E;
    const float c1 = 2.0f * B * Cc - 4.0f * R2 * F;
    const float c0 = Cc * Cc - 4.0f * R2 * G;

    const QuarticRoots roots = solveQuartic(c4, c3, c2, c1, c0);

    float bestT = 1e30f;
    for (int i = 0; i < roots.count; ++i) {
        const float t = roots.x[i];
        if (t > 1e-4f && t < bestT) {
            bestT = t;
        }
    }
    if (bestT >= 1e29f) {
        return best;
    }

    best.hit = true;
    best.t = bestT;
    best.point = from + dir * bestT;
    // Local hit point -> nearest point on the tube centre circle -> outward normal.
    const vec3 pl = o + d * bestT;
    const float rho = std::sqrt(pl.x * pl.x + pl.y * pl.y);
    vec3 nLocal;
    if (rho > 1e-6f) {
        const vec3 circle(R * pl.x / rho, R * pl.y / rho, 0.0f); // nearest centre-circle point
        nLocal = pl - circle;
    } else {
        nLocal = vec3(0.0f, 0.0f, pl.z >= 0.0f ? 1.0f : -1.0f);
    }
    const float nl = std::sqrt(dot(nLocal, nLocal));
    if (nl > 1e-9f) {
        nLocal = nLocal * (1.0f / nl);
    }
    best.normal = u * nLocal.x + v * nLocal.y + w * nLocal.z; // back to world
    return best;
}

} // namespace maz::math
