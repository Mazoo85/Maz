#pragma once

#include "maz/math/Math.hpp"      // vec2, dot
#include "maz/math/VectorOps.hpp" // lengthSquared

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::math penetration query — the companion to GJK distance that answers the OTHER half of convex-vs-convex
// collision: when two convex polygons OVERLAP, how deep, and which way do you push to separate them?
// GjkDistance.hpp reports the gap between shapes that are APART (and 0 when they touch); this takes over once
// they interpenetrate and returns the PENETRATION DEPTH and CONTACT NORMAL — the minimum translation vector
// (MTV) that pushes shape A just clear of shape B. That is exactly what a rigid-body solver needs to resolve
// a collision between two arbitrary convex shapes (not just the box/circle special cases SAT hand-codes).
// It works on the Minkowski difference A (-) B = { a - b }: the two shapes overlap iff that (convex) set
// contains the origin, and the shortest way out is the closest point on its boundary. In 2D the exact
// boundary is cheap — the convex hull of all pairwise vertex differences — so instead of the iterative EPA
// polytope expansion this computes the difference polygon directly and takes the closest edge to the origin,
// giving an EXACT depth and normal (no convergence tolerance). Pure vec2 math, deterministic, header-only.
// Godot exposes no such query. (Convex inputs assumed; any vertex order works.)
namespace maz::math {

struct EpaResult {
    bool intersecting = false; // true when the shapes overlap
    float depth = 0.0f;        // penetration depth (0 when not intersecting)
    vec2 normal{0.0f, 0.0f};   // unit MTV direction — move A by +depth*normal to separate it from B
};

namespace detail {

// Andrew's monotone-chain convex hull -> CCW, no collinear interior points. Fewer than 3 unique points
// yields the input (degenerate).
inline std::vector<vec2> convexHullCcw(std::vector<vec2> pts) {
    std::sort(pts.begin(), pts.end(), [](const vec2& p, const vec2& q) {
        return p.x < q.x || (p.x == q.x && p.y < q.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end(),
                          [](const vec2& p, const vec2& q) { return p.x == q.x && p.y == q.y; }),
              pts.end());
    const std::size_t n = pts.size();
    if (n < 3) {
        return pts;
    }
    auto crossz = [](const vec2& o, const vec2& a, const vec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<vec2> hull(2 * n);
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) { // lower hull
        while (k >= 2 && crossz(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) --k;
        hull[k++] = pts[i];
    }
    const std::size_t lower = k + 1;
    for (std::size_t i = n - 1; i-- > 0;) { // upper hull
        while (k >= lower && crossz(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) --k;
        hull[k++] = pts[i];
    }
    hull.resize(k - 1);
    return hull;
}

} // namespace detail

// Compute penetration depth + separating normal for two overlapping convex polygons. If they do not overlap,
// returns {intersecting=false}. Winding of the inputs does not matter.
inline EpaResult epaPenetration(const std::vector<vec2>& a, const std::vector<vec2>& b) {
    EpaResult res;
    if (a.size() < 3 || b.size() < 3) {
        return res;
    }
    // Minkowski difference point cloud, then its exact (convex) boundary.
    std::vector<vec2> diff;
    diff.reserve(a.size() * b.size());
    for (const vec2& pa : a) {
        for (const vec2& pb : b) {
            diff.push_back(vec2{pa.x - pb.x, pa.y - pb.y});
        }
    }
    const std::vector<vec2> hull = detail::convexHullCcw(std::move(diff));
    const std::size_t n = hull.size();
    if (n < 3) {
        return res;
    }
    // Origin inside the CCW hull iff it is on the left of every directed edge. While scanning, track the
    // closest edge (outward-normal distance) for the penetration depth.
    float minDist = 1e30f;
    vec2 minOutward{0.0f, 0.0f};
    for (std::size_t i = 0; i < n; ++i) {
        const vec2 p0 = hull[i];
        const vec2 p1 = hull[(i + 1) % n];
        const vec2 e = p1 - p0;
        // Left-of test for the origin: cross(e, O - p0) >= 0 for a CCW polygon.
        const float side = e.x * (0.0f - p0.y) - e.y * (0.0f - p0.x);
        if (side < -1e-6f) {
            return res; // origin outside -> no overlap
        }
        // Outward (right-hand) normal of a CCW edge is (e.y, -e.x); distance from origin = dot(n_unit, p0).
        vec2 outward = vec2{e.y, -e.x};
        const float ln = std::sqrt(lengthSquared(outward));
        if (ln < 1e-12f) {
            continue;
        }
        outward = vec2{outward.x / ln, outward.y / ln};
        const float d = dot(outward, p0);
        if (d < minDist) {
            minDist = d;
            minOutward = outward;
        }
    }
    res.intersecting = true;
    res.depth = minDist < 0.0f ? 0.0f : minDist;
    // The closest boundary point sits at +depth*outward from the origin; to push A CLEAR of B translate A the
    // opposite way (verified on the axis-aligned box overlap case).
    res.normal = vec2{-minOutward.x, -minOutward.y};
    return res;
}

} // namespace maz::math
