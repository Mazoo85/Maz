#pragma once

// The 2D convex hull of a point set via Andrew's monotone chain — returns the
// hull vertices counter-clockwise, with collinear edge points excluded (minimal
// hull) and no repeated closing vertex. Fewer than 3 unique points return as-is
// (a point or a segment). Deterministic (the hull starts at the lexicographically
// -smallest point). O(n log n) (dominated by the sort). The Godot collision-shape
// / convex-hull analog; composes maz::math vec2. Works in the XY plane (2D).
// 3D hull / convex decomposition are future refinements.

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace maz::math {

// Compute the 2D convex hull of a point set (Andrew's monotone chain). Takes the
// points by value (it sorts/dedupes a local copy). Returns the hull vertices in
// counter-clockwise order, without collinear points on the edges (minimal hull),
// and without a duplicated closing vertex (the first vertex is not repeated).
inline std::vector<vec2> convexHull2D(std::vector<vec2> points) {
    // Sort by (x, then y) so the sweep runs left-to-right.
    std::sort(points.begin(), points.end(),
              [](vec2 a, vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });

    // Dedupe exact duplicates after the sort.
    points.erase(std::unique(points.begin(), points.end(),
                             [](vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }),
                 points.end());

    // Degenerate: 0, 1, or 2 unique points are already the answer (a point or a
    // segment) — return them sorted/deduped.
    if (points.size() < 3) {
        return points;
    }

    // 2D scalar cross of OA x OB (positive = counter-clockwise / left turn).
    auto cross = [](vec2 o, vec2 a, vec2 b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };

    std::vector<vec2> hull;

    // Lower hull, left-to-right. The `<= 0` pops collinear AND right-turn points,
    // so collinear edge points are excluded (minimal hull).
    for (const vec2& p : points) {
        while (hull.size() >= 2 &&
               cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0.0f) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    // Upper hull, right-to-left, continuing on the same vector. Anything at or
    // below the lower-hull size must be preserved. This sweeps the full reverse
    // range (rbegin..rend): the first iteration re-pushes the rightmost point,
    // which is immediately popped on the next step because cross(o, o, p) == 0
    // satisfies the `<= 0` test — so the `<=` (collinear-excluding) comparison is
    // load-bearing here; changing it to `<` would leave that duplicate in place.
    const std::size_t lower = hull.size() + 1;
    for (auto it = points.rbegin(); it != points.rend(); ++it) {
        const vec2& p = *it;
        while (hull.size() >= lower &&
               cross(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0.0f) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    // The last point duplicates the first (the sweep returns to the start).
    hull.pop_back();
    return hull;
}

} // namespace maz::math
