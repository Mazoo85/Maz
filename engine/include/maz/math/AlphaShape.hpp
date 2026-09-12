#pragma once

#include "maz/math/Delaunay.hpp" // triangulateDelaunay, vec2

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::math alpha shapes / concave hull — the "shrink-wrap" outline of a scattered 2D point set. A convex
// hull is the tightest CONVEX rubber band around the points; it can never dip into a bay or wrap around a
// C-shape. The alpha shape does: it keeps only the Delaunay triangles small enough to fit a disc of radius
// `alpha`, so any gap wider than ~2*alpha is left OUTSIDE the shape — carving out concavities, notches and
// holes. Sweep alpha from large to small and the outline morphs from the convex hull down to the bare
// points. This is the standard tool for turning a cloud of samples (a scanned blob, a splatter of hit
// points, a territory of unit positions, a lasso selection) into a real polygon you can fill, collide, or
// path around. `alphaShapeEdges` returns the boundary edges; `concaveHull` walks the outer boundary into an
// ordered CCW polygon. Godot ships convex hulls only, so the concave case is a beyond-Godot geometry
// utility. Header-only, std-only, deterministic; builds on triangulateDelaunay.
namespace maz::math {

// An undirected boundary edge, as a pair of indices into the source point list (a < b).
struct AlphaEdge {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
};

namespace detail {

// Circumradius of triangle ABC. Returns a very large value for degenerate (near-collinear) triangles so
// the alpha test drops them unless alpha is effectively infinite.
inline float alphaCircumRadius(const vec2& A, const vec2& B, const vec2& C) {
    const float a = std::sqrt((B.x - C.x) * (B.x - C.x) + (B.y - C.y) * (B.y - C.y));
    const float b = std::sqrt((C.x - A.x) * (C.x - A.x) + (C.y - A.y) * (C.y - A.y));
    const float c = std::sqrt((A.x - B.x) * (A.x - B.x) + (A.y - B.y) * (A.y - B.y));
    const float area2 = std::fabs((B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x)); // 2*area
    if (area2 < 1e-12f) {
        return 1e30f;
    }
    return (a * b * c) / (2.0f * area2);
}

inline std::uint64_t alphaEdgeKey(std::uint32_t lo, std::uint32_t hi) {
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

} // namespace detail

// Boundary edges of the alpha shape: triangulate, keep every triangle whose circumradius <= alpha, and
// return the edges that bound exactly one kept triangle. `alpha <= 0` means "infinite" (the convex-hull
// boundary). Fewer than three points, or fully collinear input, yields no edges.
inline std::vector<AlphaEdge> alphaShapeEdges(const std::vector<vec2>& points, float alpha) {
    std::vector<AlphaEdge> out;
    const std::vector<std::uint32_t> tris = triangulateDelaunay(points);
    if (tris.empty()) {
        return out;
    }
    const float lim = alpha > 0.0f ? alpha : 1e30f;
    // Count how many KEPT triangles each undirected edge belongs to.
    std::unordered_map<std::uint64_t, int> edgeCount;
    edgeCount.reserve(tris.size());
    auto addEdge = [&](std::uint32_t u, std::uint32_t v) {
        const std::uint32_t lo = u < v ? u : v, hi = u < v ? v : u;
        ++edgeCount[detail::alphaEdgeKey(lo, hi)];
    };
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        const std::uint32_t ia = tris[t], ib = tris[t + 1], ic = tris[t + 2];
        const float R = detail::alphaCircumRadius(points[ia], points[ib], points[ic]);
        if (R <= lim) {
            addEdge(ia, ib);
            addEdge(ib, ic);
            addEdge(ic, ia);
        }
    }
    for (const auto& kv : edgeCount) {
        if (kv.second == 1) {
            out.push_back(AlphaEdge{static_cast<std::uint32_t>(kv.first >> 32),
                                    static_cast<std::uint32_t>(kv.first & 0xffffffffu)});
        }
    }
    return out;
}

// Outer boundary of the alpha shape, as an ordered CCW loop of indices into `points`. Walks the boundary
// edges starting from the lowest point (guaranteed on the outer ring). Returns empty if the outer boundary
// is not a single simple loop (e.g. a pinch point where a boundary vertex has degree != 2). Interior hole
// loops, if any, are intentionally not returned — this is the outer concave hull.
inline std::vector<std::uint32_t> concaveHull(const std::vector<vec2>& points, float alpha) {
    std::vector<std::uint32_t> ring;
    const std::vector<AlphaEdge> edges = alphaShapeEdges(points, alpha);
    if (edges.size() < 3) {
        return ring;
    }
    // Adjacency from the undirected boundary edges.
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> adj;
    adj.reserve(edges.size() * 2);
    for (const AlphaEdge& e : edges) {
        adj[e.a].push_back(e.b);
        adj[e.b].push_back(e.a);
    }
    // Start at the lowest (then leftmost) boundary vertex — always on the outer ring.
    std::uint32_t start = edges[0].a;
    bool haveStart = false;
    for (const auto& kv : adj) {
        const vec2& p = points[kv.first];
        if (!haveStart) {
            start = kv.first;
            haveStart = true;
            continue;
        }
        const vec2& s = points[start];
        if (p.y < s.y || (p.y == s.y && p.x < s.x)) {
            start = kv.first;
        }
    }
    // Walk: at each degree-2 vertex, step to the neighbour we did not come from.
    std::uint32_t prev = 0xffffffffu, cur = start;
    const std::size_t maxSteps = adj.size() + 1;
    for (std::size_t step = 0; step < maxSteps; ++step) {
        ring.push_back(cur);
        const std::vector<std::uint32_t>& nb = adj[cur];
        if (nb.size() != 2) {
            return {}; // not a simple loop (pinch / dangling)
        }
        const std::uint32_t nxt = (nb[0] != prev) ? nb[0] : nb[1];
        prev = cur;
        cur = nxt;
        if (cur == start) {
            break;
        }
    }
    if (cur != start || ring.size() < 3) {
        return {};
    }
    // Orient CCW (positive signed area).
    double area2 = 0.0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const vec2& p = points[ring[i]];
        const vec2& q = points[ring[(i + 1) % ring.size()]];
        area2 += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
    }
    if (area2 < 0.0) {
        std::reverse(ring.begin(), ring.end());
    }
    return ring;
}

} // namespace maz::math
