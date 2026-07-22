#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

// maz::render MESH GEODESIC DISTANCE — the shortest "walk along the surface" distance from one or more source
// vertices to every other vertex, measured along the mesh's EDGES (Dijkstra on the vertex graph, edge weight =
// the 3D length of that edge). Straight-line distance cuts through the solid; geodesic distance is how far it
// actually is over the skin, which is what you want for: heat-map / falloff vertex weights (damage spreading
// from a wound, snow accumulating from a peak), texture-blend and vertex-paint masks that follow the form,
// region growing / "flood N metres from here" selection, feature-distance fields, and cheap procedural effects.
// Multi-source seeds every source at 0 in one pass (a discrete distance-to-nearest-feature field). The
// predecessor array reconstructs the actual shortest edge-path back to a source. Header-only, std-only,
// deterministic.
//
// Scope note (honest): this is the EDGE-graph geodesic — the standard cheap approximation. It can only step
// vertex-to-vertex, so on a coarse mesh it slightly OVERESTIMATES the true smooth surface geodesic (which may
// cross triangle faces); it converges as the mesh is refined. Exact polyhedral geodesics (MMP / heat method)
// are the documented follow-up. Weld the mesh first (a shared corner must be one vertex) or islands stay
// disconnected — compose with MeshCleanup / MeshWeld.
namespace maz::render {

struct GeodesicResult {
    std::vector<float> distance;         // per-vertex geodesic distance from the nearest source (inf if unreachable)
    std::vector<std::uint32_t> previous; // predecessor on the shortest path (kNone at sources / unreachable)
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;

    bool reachable(std::uint32_t v) const {
        return v < distance.size() && std::isfinite(distance[v]);
    }
    // Reconstruct the vertex path from `target` back to its source (target-first order); empty if unreachable.
    std::vector<std::uint32_t> pathFrom(std::uint32_t target) const {
        std::vector<std::uint32_t> path;
        if (!reachable(target)) return path;
        for (std::uint32_t v = target; v != kNone; v = previous[v]) path.push_back(v);
        return path;
    }
};

// Geodesic distance from every vertex in `sources` (all seeded at distance 0) to all vertices. Out-of-range
// source indices are ignored; if no valid source remains, all distances are +inf.
inline GeodesicResult geodesicDistance(const shapes::MeshData& mesh,
                                       const std::vector<std::uint32_t>& sources) {
    GeodesicResult r;
    const std::size_t vn = mesh.vertices.size();
    r.distance.assign(vn, std::numeric_limits<float>::infinity());
    r.previous.assign(vn, GeodesicResult::kNone);
    if (vn == 0) return r;

    // Build the undirected weighted adjacency from triangle edges (duplicates are harmless for Dijkstra).
    std::vector<std::vector<std::pair<std::uint32_t, float>>> adj(vn);
    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return std::array<float, 3>{v.px, v.py, v.pz};
    };
    auto edgeLen = [&](std::uint32_t a, std::uint32_t b) {
        const auto pa = pos(a), pb = pos(b);
        const float dx = pa[0] - pb[0], dy = pa[1] - pb[1], dz = pa[2] - pb[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t a = mesh.indices[t * 3 + 0];
        const std::uint32_t b = mesh.indices[t * 3 + 1];
        const std::uint32_t c = mesh.indices[t * 3 + 2];
        if (a >= vn || b >= vn || c >= vn) continue;
        const float wab = edgeLen(a, b), wbc = edgeLen(b, c), wca = edgeLen(c, a);
        adj[a].push_back({b, wab}); adj[b].push_back({a, wab});
        adj[b].push_back({c, wbc}); adj[c].push_back({b, wbc});
        adj[c].push_back({a, wca}); adj[a].push_back({c, wca});
    }

    using Node = std::pair<float, std::uint32_t>; // (distance, vertex)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    for (std::uint32_t s : sources) {
        if (s < vn && r.distance[s] != 0.0f) {
            r.distance[s] = 0.0f;
            r.previous[s] = GeodesicResult::kNone;
            pq.push({0.0f, s});
        }
    }

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();
        if (d > r.distance[u]) continue; // stale heap entry
        for (const auto& [v, w] : adj[u]) {
            const float nd = d + w;
            if (nd < r.distance[v]) {
                r.distance[v] = nd;
                r.previous[v] = u;
                pq.push({nd, v});
            }
        }
    }
    return r;
}

// Single-source convenience overload.
inline GeodesicResult geodesicDistance(const shapes::MeshData& mesh, std::uint32_t source) {
    return geodesicDistance(mesh, std::vector<std::uint32_t>{source});
}

} // namespace maz::render
