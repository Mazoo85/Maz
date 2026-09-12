#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>

// maz::math closest point on / distance to an axis-aligned ellipse — the robust query "what is the nearest
// point on the ellipse boundary to p, and how far is it?", plus the SIGNED distance (negative inside). The
// distance-to-an-ellipse is a genuinely hard little problem: unlike a circle there is no closed form, and the
// naive "project along the radius" is wrong everywhere except the axes. This uses Eberly's robust method —
// reduce to the first quadrant, then bisect a single monotone function for the foot of the perpendicular —
// which stays accurate even near the flat sides and the sharp ends where iterative solvers usually degrade.
// The engine has parametric ellipse points (Superellipse.hpp) but no distance/closest-point query; Godot has
// none either. Uses: snapping to elliptical orbits/tracks, elliptical soft-body/collision response, GUI
// hit-testing against ovals, and an exact elliptical signed-distance field. Header-only, std-only,
// deterministic.
namespace maz::math {

namespace ellipse_detail {
// Bisection for the root s of g(s) = (r0*z0/(s+r0))^2 + (z1/(s+1))^2 - 1, with z0,z1 >= 0, r0 = (e0/e1)^2.
// g is strictly decreasing on the bracket, so a fixed sweep of bisections converges deterministically.
inline float getRoot(float r0, float z0, float z1, float g) {
    const float n0 = r0 * z0;
    float s0 = z1 - 1.0f;
    float s1 = (g < 0.0f) ? 0.0f : (std::sqrt(n0 * n0 + z1 * z1) - 1.0f);
    float s = 0.0f;
    for (int i = 0; i < 60; ++i) {
        s = 0.5f * (s0 + s1);
        if (s == s0 || s == s1) {
            break;
        }
        const float ratio0 = n0 / (s + r0);
        const float ratio1 = z1 / (s + 1.0f);
        const float gg = ratio0 * ratio0 + ratio1 * ratio1 - 1.0f;
        if (gg > 0.0f) {
            s0 = s;
        } else if (gg < 0.0f) {
            s1 = s;
        } else {
            break;
        }
    }
    return s;
}

// Eberly's first-quadrant solver: semi-axes e0 >= e1 > 0, query (y0, y1) with y0, y1 >= 0. Writes the closest
// boundary point (x0, x1) and returns the (unsigned) distance.
inline float distancePointEllipseQ1(float e0, float e1, float y0, float y1, float& x0, float& x1) {
    if (y1 > 0.0f) {
        if (y0 > 0.0f) {
            const float z0 = y0 / e0, z1 = y1 / e1;
            const float g = z0 * z0 + z1 * z1 - 1.0f;
            if (g != 0.0f) {
                const float r0 = (e0 / e1) * (e0 / e1);
                const float sbar = getRoot(r0, z0, z1, g);
                x0 = r0 * y0 / (sbar + r0);
                x1 = y1 / (sbar + 1.0f);
                const float dx = x0 - y0, dy = x1 - y1;
                return std::sqrt(dx * dx + dy * dy);
            }
            x0 = y0;
            x1 = y1;
            return 0.0f;
        }
        // y0 == 0: closest point is the minor-axis vertex.
        x0 = 0.0f;
        x1 = e1;
        return std::fabs(y1 - e1);
    }
    // y1 == 0: on the major axis — either the perpendicular foot on the flank, or the vertex.
    const float numer0 = e0 * y0;
    const float denom0 = e0 * e0 - e1 * e1;
    if (numer0 < denom0) {
        const float xde0 = numer0 / denom0;
        x0 = e0 * xde0;
        const float t = 1.0f - xde0 * xde0;
        x1 = e1 * std::sqrt(t > 0.0f ? t : 0.0f);
        const float dx = x0 - y0;
        return std::sqrt(dx * dx + x1 * x1);
    }
    x0 = e0;
    x1 = 0.0f;
    return std::fabs(y0 - e0);
}
} // namespace ellipse_detail

// The closest point on the axis-aligned ellipse of semi-axes (a, b), centred at the origin, to point `p`.
// `a`, `b` must be > 0. Robust for p inside, outside, on an axis, or at the centre.
inline vec2 closestPointOnEllipse(float a, float b, const vec2& p) {
    // Reduce to the first quadrant; remember the signs to restore afterwards.
    const float sx = p.x < 0.0f ? -1.0f : 1.0f;
    const float sy = p.y < 0.0f ? -1.0f : 1.0f;
    const float px = std::fabs(p.x), py = std::fabs(p.y);
    float x0 = 0.0f, x1 = 0.0f;
    if (a >= b) {
        ellipse_detail::distancePointEllipseQ1(a, b, px, py, x0, x1);
        return vec2(sx * x0, sy * x1);
    }
    // Major axis is y: solve with axes/coords swapped, then swap the result back.
    ellipse_detail::distancePointEllipseQ1(b, a, py, px, x0, x1);
    return vec2(sx * x1, sy * x0);
}

// The unsigned distance from `p` to the ellipse boundary (0 on the boundary).
inline float distanceToEllipse(float a, float b, const vec2& p) {
    const vec2 c = closestPointOnEllipse(a, b, p);
    const float dx = p.x - c.x, dy = p.y - c.y;
    return std::sqrt(dx * dx + dy * dy);
}

// The SIGNED distance to the ellipse: negative inside, positive outside, 0 on the boundary. This is the exact
// elliptical SDF (unlike the common cheap "gradient-scaled implicit", which is only approximate).
inline float signedDistanceEllipse(float a, float b, const vec2& p) {
    const float d = distanceToEllipse(a, b, p);
    const float inside = (p.x / a) * (p.x / a) + (p.y / b) * (p.y / b) - 1.0f; // <0 inside
    return inside < 0.0f ? -d : d;
}

} // namespace maz::math
