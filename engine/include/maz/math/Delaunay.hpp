#pragma once

#include "maz/math/Math.hpp" // math::vec2

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

// maz::math Delaunay triangulation — Godot's Geometry2D.triangulate_delaunay. Given a set of 2D points
// it returns the Delaunay triangulation as a flat list of vertex indices (three per triangle, indexing
// the input). The Delaunay property (no point lies inside any triangle's circumcircle) yields the
// "roundest" triangles, which is what you want for terrain meshes, procedural tessellation, path
// graphs, and dual-Voronoi work. Implemented with Bowyer-Watson incremental insertion and a robust
// double-precision in-circle predicate. Pure, header-only, deterministic — unit-tests exactly.
namespace maz::math {

namespace detail {

struct DelTri {
    int a, b, c;
};

// True if p lies strictly inside the circumcircle of triangle (a,b,c), assumed CCW. Double precision
// for a stable predicate near cocircular configurations.
inline bool delInCircle(const vec2& a, const vec2& b, const vec2& c, const vec2& p) {
    const double ax = static_cast<double>(a.x) - p.x;
    const double ay = static_cast<double>(a.y) - p.y;
    const double bx = static_cast<double>(b.x) - p.x;
    const double by = static_cast<double>(b.y) - p.y;
    const double cx = static_cast<double>(c.x) - p.x;
    const double cy = static_cast<double>(c.y) - p.y;
    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    const double det = ax * (by * c2 - b2 * cy) - ay * (bx * c2 - b2 * cx) + a2 * (bx * cy - by * cx);
    return det > 0.0;
}

inline double delCross(const vec2& o, const vec2& a, const vec2& b) {
    return (static_cast<double>(a.x) - o.x) * (static_cast<double>(b.y) - o.y) -
           (static_cast<double>(a.y) - o.y) * (static_cast<double>(b.x) - o.x);
}

} // namespace detail

// Triangulate `points`. Returns indices into `points`, three per triangle (CCW). Fewer than 3 points
// or fully collinear input returns empty.
inline std::vector<std::uint32_t> triangulateDelaunay(const std::vector<vec2>& points) {
    std::vector<std::uint32_t> out;
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        return out;
    }

    // Augmented point list: inputs + 3 super-triangle corners enclosing everything.
    std::vector<vec2> pts = points;
    float minx = points[0].x, miny = points[0].y, maxx = points[0].x, maxy = points[0].y;
    for (const vec2& p : points) {
        minx = p.x < minx ? p.x : minx;
        miny = p.y < miny ? p.y : miny;
        maxx = p.x > maxx ? p.x : maxx;
        maxy = p.y > maxy ? p.y : maxy;
    }
    const float dx = maxx - minx;
    const float dy = maxy - miny;
    const float dmax = (dx > dy ? dx : dy);
    if (dmax <= 0.0f) {
        return out; // all points coincident
    }
    const float midx = (minx + maxx) * 0.5f;
    const float midy = (miny + maxy) * 0.5f;
    const float big = dmax * 20.0f;
    pts.push_back(vec2(midx - big, midy - dmax));
    pts.push_back(vec2(midx, midy + big));
    pts.push_back(vec2(midx + big, midy - dmax));
    const int s0 = n, s1 = n + 1, s2 = n + 2;

    auto tri = [&](int a, int b, int c) {
        // Store CCW.
        if (detail::delCross(pts[static_cast<std::size_t>(a)], pts[static_cast<std::size_t>(b)],
                             pts[static_cast<std::size_t>(c)]) < 0.0) {
            std::swap(b, c);
        }
        return detail::DelTri{a, b, c};
    };

    std::vector<detail::DelTri> tris;
    tris.push_back(tri(s0, s1, s2));

    // Insert each input point.
    for (int i = 0; i < n; ++i) {
        const vec2& p = points[static_cast<std::size_t>(i)];
        // Find triangles whose circumcircle contains p; collect their edges.
        std::vector<std::array<int, 2>> edges;
        std::vector<detail::DelTri> kept;
        kept.reserve(tris.size());
        for (const detail::DelTri& t : tris) {
            if (detail::delInCircle(pts[static_cast<std::size_t>(t.a)], pts[static_cast<std::size_t>(t.b)],
                                    pts[static_cast<std::size_t>(t.c)], p)) {
                edges.push_back({t.a, t.b});
                edges.push_back({t.b, t.c});
                edges.push_back({t.c, t.a});
            } else {
                kept.push_back(t);
            }
        }
        // Boundary edges appear exactly once (shared edges cancel). Compare undirected.
        tris = kept;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            bool shared = false;
            for (std::size_t f = 0; f < edges.size(); ++f) {
                if (e == f) {
                    continue;
                }
                if ((edges[e][0] == edges[f][0] && edges[e][1] == edges[f][1]) ||
                    (edges[e][0] == edges[f][1] && edges[e][1] == edges[f][0])) {
                    shared = true;
                    break;
                }
            }
            if (!shared) {
                tris.push_back(tri(edges[e][0], edges[e][1], i));
            }
        }
    }

    // Drop triangles touching the super-triangle; emit the rest.
    for (const detail::DelTri& t : tris) {
        if (t.a >= s0 || t.b >= s0 || t.c >= s0) {
            continue;
        }
        out.push_back(static_cast<std::uint32_t>(t.a));
        out.push_back(static_cast<std::uint32_t>(t.b));
        out.push_back(static_cast<std::uint32_t>(t.c));
    }
    return out;
}

} // namespace maz::math
