#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <cstddef>
#include <vector>

// maz::math NURBS curve (Non-Uniform Rational B-Spline) — the industry-standard freeform curve used by every
// CAD tool and vector program. It generalises the engine's plain B-spline by giving each control point a
// WEIGHT, which lets a single curve type represent EXACT conics — perfect circles, ellipses and arcs — that
// no polynomial Bezier or B-spline can reproduce, alongside arbitrary smooth freeform shapes. Non-uniform
// knots let you place sharper or gentler regions and pin the endpoints. Use it for precise vector paths,
// smooth camera/motion rails that must pass through exact circular arcs, lofting profiles, and authoring
// tools. Evaluated by the numerically stable de Boor algorithm on homogeneous (weighted) control points,
// then perspective-divided back. Godot's Curve2D is cubic Bezier only — no rational curves. Header-only,
// std-only, deterministic.
namespace maz::math {

// Evaluate a NURBS curve of the given `degree` at parameter `u`. `ctrl` and `weights` have equal length n;
// `knots` has length n + degree + 1. Returns the point on the curve. Malformed inputs return (0,0).
inline vec2 nurbsPoint(const std::vector<vec2>& ctrl, const std::vector<float>& weights,
                       const std::vector<float>& knots, int degree, float u) {
    const int nc = static_cast<int>(ctrl.size());
    const int p = degree;
    if (nc < p + 1 || p < 1 || weights.size() != ctrl.size() ||
        knots.size() != static_cast<std::size_t>(nc + p + 1)) {
        return vec2(0.0f, 0.0f);
    }
    const std::vector<float>& U = knots;
    // Find the knot span containing u (clamped to the valid domain [U[p], U[nc]]).
    int span;
    if (u <= U[static_cast<std::size_t>(p)]) {
        span = p;
    } else if (u >= U[static_cast<std::size_t>(nc)]) {
        span = nc - 1;
    } else {
        span = p;
        while (span < nc - 1 && u >= U[static_cast<std::size_t>(span + 1)]) {
            ++span;
        }
    }
    // de Boor on homogeneous control points (w*x, w*y, w).
    std::vector<vec3> d(static_cast<std::size_t>(p + 1));
    for (int j = 0; j <= p; ++j) {
        const int i = span - p + j;
        const float w = weights[static_cast<std::size_t>(i)];
        d[static_cast<std::size_t>(j)] = vec3(ctrl[static_cast<std::size_t>(i)].x * w,
                                              ctrl[static_cast<std::size_t>(i)].y * w, w);
    }
    for (int r = 1; r <= p; ++r) {
        for (int j = p; j >= r; --j) {
            const int i = span - p + j;
            const float denom = U[static_cast<std::size_t>(i + p - r + 1)] - U[static_cast<std::size_t>(i)];
            const float a = denom > 1e-9f ? (u - U[static_cast<std::size_t>(i)]) / denom : 0.0f;
            d[static_cast<std::size_t>(j)] = d[static_cast<std::size_t>(j - 1)] * (1.0f - a) +
                                            d[static_cast<std::size_t>(j)] * a;
        }
    }
    const vec3 h = d[static_cast<std::size_t>(p)];
    if (h.z <= 1e-9f && h.z >= -1e-9f) {
        return vec2(0.0f, 0.0f);
    }
    return vec2(h.x / h.z, h.y / h.z);
}

// Convenience: a clamped uniform knot vector [0..1] for `nCtrl` control points of the given `degree`
// (p+1 zeros, then the interior knots evenly spaced, then p+1 ones).
inline std::vector<float> nurbsClampedKnots(int nCtrl, int degree) {
    std::vector<float> U;
    const int p = degree;
    const int m = nCtrl + p + 1;
    U.reserve(static_cast<std::size_t>(m));
    for (int i = 0; i < m; ++i) {
        if (i <= p) {
            U.push_back(0.0f);
        } else if (i >= nCtrl) {
            U.push_back(1.0f);
        } else {
            U.push_back(static_cast<float>(i - p) / static_cast<float>(nCtrl - p));
        }
    }
    return U;
}

} // namespace maz::math
