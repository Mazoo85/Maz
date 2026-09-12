#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>

// maz::math inverse bilinear interpolation — map a point inside a (possibly warped) quad back to its
// (u,v) coordinates in the unit square.
//
// FORWARD bilinear is easy: given (u,v) in [0,1]^2 and the four quad corners, blend them. The INVERSE —
// "I have a point P inside this quad; what (u,v) produced it?" — is what you need to look up the UV/colour
// of a hit position in a warped or perspective-flattened quad, to deform a grid, to map screen-picks into
// a distorted panel's local space, or to resample between two non-aligned grids. For a general
// (non-parallelogram) quad the answer requires solving a quadratic, which this does robustly (falling
// back to the linear affine case when the quad is a parallelogram). Corners are A(0,0) B(1,0) C(1,1)
// D(0,1) going around. Pure vec2 math, header-only, deterministic — unit-tested by round-tripping the
// forward map and against the exact corner/centre coordinates.
namespace maz::math {

// Forward bilinear: the point at parameters (u,v) in the quad A(0,0) B(1,0) C(1,1) D(0,1).
inline vec2 bilinear(const vec2& a, const vec2& b, const vec2& c, const vec2& d, float u, float v) {
    const float w00 = (1.0f - u) * (1.0f - v);
    const float w10 = u * (1.0f - v);
    const float w11 = u * v;
    const float w01 = (1.0f - u) * v;
    return vec2(w00 * a.x + w10 * b.x + w11 * c.x + w01 * d.x,
                w00 * a.y + w10 * b.y + w11 * c.y + w01 * d.y);
}

struct InvBilinearResult {
    vec2 uv{0.0f, 0.0f};
    bool valid = false; // false if p is not expressible inside the quad (no real/in-range solution)
};

namespace detail {
inline float ibCross(const vec2& u, const vec2& v) { return u.x * v.y - u.y * v.x; }
} // namespace detail

// Recover (u,v) for point `p` in the quad A B C D (order A(0,0) B(1,0) C(1,1) D(0,1)). Íñigo Quílez's
// robust formulation. `valid` is set only when a solution with u,v in [-eps,1+eps] exists.
inline InvBilinearResult invBilinear(const vec2& p, const vec2& a, const vec2& b, const vec2& c,
                                     const vec2& d, float eps = 1e-4f) {
    const vec2 e(b.x - a.x, b.y - a.y);
    const vec2 f(d.x - a.x, d.y - a.y);
    const vec2 g(a.x - b.x + c.x - d.x, a.y - b.y + c.y - d.y);
    const vec2 h(p.x - a.x, p.y - a.y);

    const float k2 = detail::ibCross(g, f);
    const float k1 = detail::ibCross(e, f) + detail::ibCross(h, g);
    const float k0 = detail::ibCross(h, e);

    InvBilinearResult out;

    // Solve for u along a direction that has a non-degenerate denominator (e + g*v), picking x or y.
    auto solveU = [&](float v) -> float {
        const float denomX = e.x + g.x * v;
        const float denomY = e.y + g.y * v;
        if (std::fabs(denomX) >= std::fabs(denomY)) {
            return (h.x - f.x * v) / denomX;
        }
        return (h.y - f.y * v) / denomY;
    };
    auto inRange = [&](float t) { return t > -eps && t < 1.0f + eps; };

    if (std::fabs(k2) < 1e-7f) {
        // Parallelogram / affine case: v is linear.
        if (std::fabs(k1) < 1e-12f) {
            return out;
        }
        const float v = -k0 / k1;
        const float u = solveU(v);
        out.uv = vec2(u, v);
        out.valid = inRange(u) && inRange(v);
        return out;
    }

    float disc = k1 * k1 - 4.0f * k0 * k2;
    if (disc < 0.0f) {
        return out; // no real solution
    }
    disc = std::sqrt(disc);
    const float v1 = (-k1 - disc) / (2.0f * k2);
    const float v2 = (-k1 + disc) / (2.0f * k2);
    const float u1 = solveU(v1);
    const float u2 = solveU(v2);
    const bool ok1 = inRange(u1) && inRange(v1);
    const bool ok2 = inRange(u2) && inRange(v2);

    if (ok1) {
        out.uv = vec2(u1, v1);
        out.valid = true;
    } else if (ok2) {
        out.uv = vec2(u2, v2);
        out.valid = true;
    } else {
        // Neither root lands inside; return the closer-to-valid root, flagged invalid.
        out.uv = vec2(u1, v1);
        out.valid = false;
    }
    return out;
}

} // namespace maz::math
