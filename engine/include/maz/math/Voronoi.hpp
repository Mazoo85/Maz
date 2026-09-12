#pragma once

#include "maz/math/Math.hpp" // math::vec2

#include <cstddef>
#include <vector>

// maz::math Voronoi diagram — the dual of Delaunay (M257). Each input "site" gets the region of the
// plane closer to it than to any other site; clipped to a bounding box you get one convex polygon per
// site. Voronoi cells drive region maps, procedural biome/territory generation, influence maps, and
// point-nearest queries. Godot ships no built-in Voronoi, so this is a beyond-parity extra. Computed
// robustly by half-plane intersection (Sutherland-Hodgman clip the box by every perpendicular
// bisector) rather than a fragile sweep — O(n^2) but exact and branch-simple. Header-only, pure,
// deterministic; unit-tests exactly (cells partition the box, each site sits in its own cell).
namespace maz::math {

namespace detail {

// Signed area * 2 of a polygon (CCW positive).
inline double polyArea2(const std::vector<vec2>& p) {
    double a = 0.0;
    for (std::size_t i = 0, n = p.size(); i < n; ++i) {
        const vec2& u = p[i];
        const vec2& v = p[(i + 1) % n];
        a += static_cast<double>(u.x) * v.y - static_cast<double>(v.x) * u.y;
    }
    return a;
}

// Clip convex polygon `poly` to the half-plane { x : dot(x - anchor, normal) <= 0 } (Sutherland-
// Hodgman). Points with side <= 0 are kept.
inline std::vector<vec2> clipHalfPlane(const std::vector<vec2>& poly, vec2 anchor, vec2 normal) {
    std::vector<vec2> out;
    const std::size_t n = poly.size();
    if (n == 0) {
        return out;
    }
    auto side = [&](const vec2& p) {
        return static_cast<double>(p.x - anchor.x) * normal.x +
               static_cast<double>(p.y - anchor.y) * normal.y;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& cur = poly[i];
        const vec2& nxt = poly[(i + 1) % n];
        const double sc = side(cur);
        const double sn = side(nxt);
        const bool curIn = sc <= 1e-9;
        const bool nxtIn = sn <= 1e-9;
        if (curIn) {
            out.push_back(cur);
        }
        if (curIn != nxtIn) {
            const double t = sc / (sc - sn); // intersection parameter along cur->nxt
            out.push_back(vec2(cur.x + static_cast<float>(t) * (nxt.x - cur.x),
                               cur.y + static_cast<float>(t) * (nxt.y - cur.y)));
        }
    }
    return out;
}

} // namespace detail

// Compute the Voronoi cell polygons for `sites`, clipped to the axis-aligned box [min,max]. Returns
// one polygon per site (CCW, possibly empty for a duplicated/covered site), index-aligned with `sites`.
inline std::vector<std::vector<vec2>> voronoiCells(const std::vector<vec2>& sites, vec2 boundsMin,
                                                   vec2 boundsMax) {
    std::vector<std::vector<vec2>> cells(sites.size());
    if (sites.empty() || boundsMax.x <= boundsMin.x || boundsMax.y <= boundsMin.y) {
        return cells;
    }
    const std::vector<vec2> box = {
        vec2(boundsMin.x, boundsMin.y), vec2(boundsMax.x, boundsMin.y),
        vec2(boundsMax.x, boundsMax.y), vec2(boundsMin.x, boundsMax.y)}; // CCW

    for (std::size_t i = 0; i < sites.size(); ++i) {
        std::vector<vec2> poly = box;
        for (std::size_t j = 0; j < sites.size() && !poly.empty(); ++j) {
            if (j == i) {
                continue;
            }
            // Keep the half-plane of points closer to site i than site j: dot(x - mid, j - i) <= 0.
            const vec2 dir(sites[j].x - sites[i].x, sites[j].y - sites[i].y);
            if (dir.x == 0.0f && dir.y == 0.0f) {
                continue; // coincident site — no separating bisector
            }
            const vec2 mid((sites[i].x + sites[j].x) * 0.5f, (sites[i].y + sites[j].y) * 0.5f);
            poly = detail::clipHalfPlane(poly, mid, dir);
        }
        cells[i] = poly;
    }
    return cells;
}

// Convenience: area of a Voronoi cell polygon (0 for empty).
inline float cellArea(const std::vector<vec2>& cell) {
    return static_cast<float>(0.5 * (detail::polyArea2(cell) < 0 ? -detail::polyArea2(cell)
                                                                 : detail::polyArea2(cell)));
}

} // namespace maz::math
