#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render Catmull-Clark subdivision — the industry-standard way to turn a blocky, low-polygon "control
// cage" of QUADS into a smooth, rounded surface, refining it one level at a time toward a limit surface.
// Where the engine's existing Loop subdivision smooths TRIANGLE meshes, Catmull-Clark works on arbitrary
// polygon faces and always outputs quads — the scheme film and modelling packages use for organic shapes
// (a cube becomes a sphere-like blob, a rough character cage becomes a clean subdivision surface). Each
// pass places one FACE point at every face's centroid, one EDGE point per edge (blending the edge's ends
// with its two neighbouring face points), and nudges every original vertex toward the average of its
// surrounding face and edge points; then every face is split into quads around those new points. Boundary
// edges/vertices use the open cubic-B-spline crease rules so borders stay put. Godot exposes no runtime
// subdivision surface, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic.
namespace maz::render {

// A polygon mesh: shared vertex positions plus faces given as ordered vertex-index loops (any arity).
struct PolyMesh {
    std::vector<math::vec3> verts;
    std::vector<std::vector<std::uint32_t>> faces;
};

namespace detail {
inline std::uint64_t ccEdgeKey(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}
struct CcEdge {
    std::uint32_t v0 = 0, v1 = 0;
    int faceCount = 0;
    math::vec3 faceSum{0.0f, 0.0f, 0.0f}; // sum of adjacent face points
    std::uint32_t index = 0;              // slot among unique edges
};
} // namespace detail

// One Catmull-Clark refinement of `in`. Returns an all-quad mesh.
inline PolyMesh catmullClarkOnce(const PolyMesh& in) {
    PolyMesh out;
    const std::size_t V = in.verts.size();
    const std::size_t F = in.faces.size();
    if (V < 3 || F == 0) {
        return in;
    }
    // 1. Face points (centroids).
    std::vector<math::vec3> facePt(F);
    for (std::size_t f = 0; f < F; ++f) {
        math::vec3 c{0.0f, 0.0f, 0.0f};
        for (std::uint32_t vi : in.faces[f]) {
            c += in.verts[vi];
        }
        facePt[f] = c / static_cast<float>(in.faces[f].size());
    }
    // 2. Edges: unique, with adjacent-face-point accumulation.
    std::unordered_map<std::uint64_t, detail::CcEdge> edges;
    edges.reserve(F * 4);
    std::uint32_t nextEdge = 0;
    for (std::size_t f = 0; f < F; ++f) {
        const std::vector<std::uint32_t>& fv = in.faces[f];
        const std::size_t k = fv.size();
        for (std::size_t i = 0; i < k; ++i) {
            const std::uint32_t a = fv[i], b = fv[(i + 1) % k];
            const std::uint64_t key = detail::ccEdgeKey(a, b);
            auto it = edges.find(key);
            if (it == edges.end()) {
                detail::CcEdge e;
                e.v0 = a;
                e.v1 = b;
                e.faceCount = 1;
                e.faceSum = facePt[f];
                e.index = nextEdge++;
                edges.emplace(key, e);
            } else {
                it->second.faceCount += 1;
                it->second.faceSum += facePt[f];
            }
        }
    }
    // Per-vertex accumulation for the vertex-point update.
    std::vector<math::vec3> vFaceSum(V, math::vec3{0.0f, 0.0f, 0.0f}); // sum of touching face points
    std::vector<int> vFaceN(V, 0);
    std::vector<math::vec3> vEdgeMidSum(V, math::vec3{0.0f, 0.0f, 0.0f}); // sum of touching edge midpoints
    std::vector<int> vEdgeN(V, 0);
    std::vector<bool> vBoundary(V, false);
    std::vector<math::vec3> vBoundarySum(V, math::vec3{0.0f, 0.0f, 0.0f}); // sum of boundary-neighbour verts
    std::vector<int> vBoundaryN(V, 0);
    for (std::size_t f = 0; f < F; ++f) {
        for (std::uint32_t vi : in.faces[f]) {
            vFaceSum[vi] += facePt[f];
            vFaceN[vi] += 1;
        }
    }
    for (const auto& kv : edges) {
        const detail::CcEdge& e = kv.second;
        const math::vec3 mid = (in.verts[e.v0] + in.verts[e.v1]) * 0.5f;
        vEdgeMidSum[e.v0] += mid;
        vEdgeN[e.v0] += 1;
        vEdgeMidSum[e.v1] += mid;
        vEdgeN[e.v1] += 1;
        if (e.faceCount == 1) { // boundary edge
            vBoundary[e.v0] = true;
            vBoundary[e.v1] = true;
            vBoundarySum[e.v0] += in.verts[e.v1];
            vBoundaryN[e.v0] += 1;
            vBoundarySum[e.v1] += in.verts[e.v0];
            vBoundaryN[e.v1] += 1;
        }
    }
    // 3. Output vertices: [0,V) updated originals, [V,V+F) face points, then edge points.
    out.verts.resize(V + F + edges.size());
    for (std::size_t v = 0; v < V; ++v) {
        if (vBoundary[v] && vBoundaryN[v] > 0) {
            // Open cubic B-spline crease rule: (6P + sum of the two boundary neighbours) / 8.
            out.verts[v] = (in.verts[v] * 6.0f + vBoundarySum[v]) * (1.0f / 8.0f);
        } else {
            const float n = static_cast<float>(vFaceN[v]);
            const math::vec3 Favg = vFaceSum[v] / n;
            const math::vec3 Ravg = vEdgeMidSum[v] / static_cast<float>(vEdgeN[v]);
            out.verts[v] = (Favg + Ravg * 2.0f + in.verts[v] * (n - 3.0f)) / n;
        }
    }
    for (std::size_t f = 0; f < F; ++f) {
        out.verts[V + f] = facePt[f];
    }
    const std::size_t edgeBase = V + F;
    for (const auto& kv : edges) {
        const detail::CcEdge& e = kv.second;
        math::vec3 ep;
        if (e.faceCount >= 2) {
            ep = (in.verts[e.v0] + in.verts[e.v1] + e.faceSum) * 0.25f;
        } else {
            ep = (in.verts[e.v0] + in.verts[e.v1]) * 0.5f; // boundary edge midpoint
        }
        out.verts[edgeBase + e.index] = ep;
    }
    // 4. New quads: for each face, one quad per corner (vertex, next-edge, face, prev-edge).
    for (std::size_t f = 0; f < F; ++f) {
        const std::vector<std::uint32_t>& fv = in.faces[f];
        const std::size_t k = fv.size();
        for (std::size_t i = 0; i < k; ++i) {
            const std::uint32_t vi = fv[i];
            const std::uint32_t vnext = fv[(i + 1) % k];
            const std::uint32_t vprev = fv[(i + k - 1) % k];
            const std::uint32_t eNext = edges.at(detail::ccEdgeKey(vi, vnext)).index;
            const std::uint32_t ePrev = edges.at(detail::ccEdgeKey(vprev, vi)).index;
            out.faces.push_back({vi, static_cast<std::uint32_t>(edgeBase) + eNext,
                                 static_cast<std::uint32_t>(V + f),
                                 static_cast<std::uint32_t>(edgeBase) + ePrev});
        }
    }
    return out;
}

// Apply `iterations` Catmull-Clark passes.
inline PolyMesh catmullClark(const PolyMesh& in, int iterations = 1) {
    PolyMesh m = in;
    for (int i = 0; i < iterations; ++i) {
        m = catmullClarkOnce(m);
    }
    return m;
}

} // namespace maz::render
