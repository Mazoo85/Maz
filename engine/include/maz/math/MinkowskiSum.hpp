#pragma once

#include "maz/math/Geometry2D.hpp" // vec2, convexHull

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::math Minkowski sum of two convex polygons — "sweep one shape around the boundary of another and
// take everything the pair can cover together": the set { a + b : a in A, b in B }. It is the workhorse
// behind collision inflation and motion planning. Grow a level's walls by the radius of the player and a
// point-sized dot can be tested instead of a fat body (the grown obstacle is exactly wall (+) player-disc).
// Build the "configuration-space obstacle" a moving convex agent must avoid (obstacle (+) reflected-agent),
// so path-planning collapses to routing a single point. Round a polygon by summing it with a small disc,
// or fatten a swept shape. The result is always convex, and its support (extent in any direction) is the
// sum of the two inputs' supports — the identity this module is verified against. Inputs are treated as
// point sets and reduced to their convex hull first, so any convex shape (or cloud of samples) works.
// Godot exposes no Minkowski sum, so this is a beyond-Godot geometry utility. Header-only, std-only,
// deterministic; O(n+m) edge merge on CCW hulls.
namespace maz::math {

// Minkowski sum of the convex hulls of A and B, returned as a CCW convex polygon. Fewer than three
// distinct points on either side (degenerate hull) yields an empty result.
inline std::vector<vec2> minkowskiSumConvex(const std::vector<vec2>& A, const std::vector<vec2>& B) {
    std::vector<vec2> P = convexHull(A); // CCW
    std::vector<vec2> Q = convexHull(B);
    if (P.size() < 3 || Q.size() < 3) {
        return {};
    }
    // Rotate each so it starts at its lowest (then leftmost) vertex — the merge assumes a common start.
    auto reorder = [](std::vector<vec2>& poly) {
        std::size_t idx = 0;
        for (std::size_t k = 1; k < poly.size(); ++k) {
            if (poly[k].y < poly[idx].y || (poly[k].y == poly[idx].y && poly[k].x < poly[idx].x)) {
                idx = k;
            }
        }
        std::rotate(poly.begin(), poly.begin() + static_cast<std::ptrdiff_t>(idx), poly.end());
    };
    reorder(P);
    reorder(Q);
    const std::size_t n = P.size(), m = Q.size();
    // Duplicate the first two vertices so edge lookups (i+1) never wrap past the sentinel.
    P.push_back(P[0]);
    P.push_back(P[1]);
    Q.push_back(Q[0]);
    Q.push_back(Q[1]);

    std::vector<vec2> res;
    res.reserve(n + m);
    std::size_t i = 0, j = 0;
    while (i < n || j < m) {
        res.push_back(vec2(P[i].x + Q[j].x, P[i].y + Q[j].y));
        // Cross of the two current edge directions decides which boundary advances (polar-angle merge).
        const float ex = (P[i + 1].x - P[i].x), ey = (P[i + 1].y - P[i].y);
        const float fx = (Q[j + 1].x - Q[j].x), fy = (Q[j + 1].y - Q[j].y);
        const float cr = ex * fy - ey * fx;
        if (cr >= 0.0f && i < n) {
            ++i;
        }
        if (cr <= 0.0f && j < m) {
            ++j;
        }
    }
    return res;
}

} // namespace maz::math
