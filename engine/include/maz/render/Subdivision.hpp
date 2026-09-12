#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "maz/math/Math.hpp"

// maz::render mesh subdivision — refine an indexed triangle mesh into a denser one, either LINEARLY
// (each triangle split into four by its edge midpoints, geometry unchanged) or with LOOP SMOOTHING (the
// standard subdivision surface: new points are weighted averages that round the silhouette toward the
// limit surface). Linear subdivision adds detail for per-vertex effects (displacement, vertex lighting,
// wave deformation) without changing the shape; Loop turns a blocky low-poly cage into a smooth organic
// form — the runtime counterpart to a modelling package's subdivision modifier. Both share the same
// topology step (every triangle becomes four, one new vertex per unique edge) and differ only in where
// the new and original vertices land. Operates on the engine's positions+indices mesh form (as used by
// MeshTools). Godot exposes no runtime subdivision to gameplay code, so this is a beyond-Godot geometry
// utility. Header-only, std-only, deterministic.
namespace maz::render {

struct SubdivMesh {
    std::vector<math::vec3> positions;
    std::vector<std::uint32_t> indices;
};

namespace detail {

inline std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t lo = a < b ? a : b;
    const std::uint32_t hi = a < b ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | hi;
}

// One subdivision pass. `smooth` selects Loop weighting; otherwise linear midpoints.
inline SubdivMesh subdivideOnce(const std::vector<math::vec3>& pos,
                                const std::vector<std::uint32_t>& idx, bool smooth) {
    using math::vec3;
    SubdivMesh out;
    const std::size_t triCount = idx.size() / 3;
    const std::uint32_t vCount = static_cast<std::uint32_t>(pos.size());

    // Gather per-edge data: the two endpoints, the opposite vertices of incident triangles, and a
    // stable new-vertex index. A boundary edge has a single opposite vertex.
    struct EdgeInfo {
        std::uint32_t a = 0, b = 0;
        std::uint32_t opp0 = 0, opp1 = 0;
        int incidence = 0;
        std::uint32_t newIndex = 0;
    };
    std::unordered_map<std::uint64_t, EdgeInfo> edges;
    edges.reserve(triCount * 3);
    auto touchEdge = [&](std::uint32_t a, std::uint32_t b, std::uint32_t opp) {
        const std::uint64_t k = edgeKey(a, b);
        auto it = edges.find(k);
        if (it == edges.end()) {
            EdgeInfo e;
            e.a = a; e.b = b; e.opp0 = opp; e.incidence = 1;
            edges.emplace(k, e);
        } else {
            if (it->second.incidence == 1) it->second.opp1 = opp;
            ++it->second.incidence;
        }
    };
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t a = idx[t * 3 + 0], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
        touchEdge(a, b, c);
        touchEdge(b, c, a);
        touchEdge(c, a, b);
    }

    // Assign edge-vertex indices (after the original vertices).
    std::uint32_t next = vCount;
    for (auto& [k, e] : edges) { (void)k; e.newIndex = next++; }

    out.positions.resize(next);

    // --- Original ("even") vertices ---
    if (!smooth) {
        for (std::uint32_t i = 0; i < vCount; ++i) out.positions[i] = pos[i];
    } else {
        // Vertex neighbour rings and boundary neighbours.
        std::vector<std::vector<std::uint32_t>> nbr(vCount);
        std::vector<std::vector<std::uint32_t>> bnd(vCount);
        for (const auto& [k, e] : edges) {
            (void)k;
            nbr[e.a].push_back(e.b);
            nbr[e.b].push_back(e.a);
            if (e.incidence == 1) { // boundary edge
                bnd[e.a].push_back(e.b);
                bnd[e.b].push_back(e.a);
            }
        }
        for (std::uint32_t i = 0; i < vCount; ++i) {
            if (bnd[i].size() == 2) {
                // Boundary vertex: 3/4 self + 1/8 each boundary neighbour.
                out.positions[i] = pos[i] * 0.75f + (pos[bnd[i][0]] + pos[bnd[i][1]]) * 0.125f;
            } else if (!nbr[i].empty() && bnd[i].empty()) {
                const std::size_t n = nbr[i].size();
                const float fn = static_cast<float>(n);
                const float t = 0.375f + 0.25f * std::cos(2.0f * 3.14159265358979f / fn);
                const float beta = (0.625f - t * t) / fn;
                vec3 sum(0.0f);
                for (std::uint32_t j : nbr[i]) sum += pos[j];
                out.positions[i] = pos[i] * (1.0f - fn * beta) + sum * beta;
            } else {
                out.positions[i] = pos[i]; // isolated or irregular boundary: leave in place
            }
        }
    }

    // --- Edge ("odd") vertices ---
    for (const auto& [k, e] : edges) {
        (void)k;
        const vec3 mid = (pos[e.a] + pos[e.b]) * 0.5f;
        if (!smooth || e.incidence == 1) {
            out.positions[e.newIndex] = mid;
        } else {
            // Interior Loop edge point: 3/8(a+b) + 1/8(c+d).
            out.positions[e.newIndex] =
                (pos[e.a] + pos[e.b]) * 0.375f + (pos[e.opp0] + pos[e.opp1]) * 0.125f;
        }
    }

    // --- New topology: each triangle becomes four ---
    out.indices.reserve(idx.size() * 4);
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t a = idx[t * 3 + 0], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
        const std::uint32_t ab = edges[edgeKey(a, b)].newIndex;
        const std::uint32_t bc = edges[edgeKey(b, c)].newIndex;
        const std::uint32_t ca = edges[edgeKey(c, a)].newIndex;
        const std::uint32_t tris[12] = {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca};
        for (std::uint32_t v : tris) out.indices.push_back(v);
    }
    return out;
}

} // namespace detail

// Linear (midpoint) subdivision: split every triangle into four by its edge midpoints. The surface is
// unchanged — new vertices lie on the original faces — but density quadruples per iteration.
inline SubdivMesh subdivideMesh(const std::vector<math::vec3>& positions,
                               const std::vector<std::uint32_t>& indices, int iterations = 1) {
    SubdivMesh m{positions, indices};
    for (int i = 0; i < iterations && !m.indices.empty(); ++i)
        m = detail::subdivideOnce(m.positions, m.indices, false);
    return m;
}

// Loop subdivision: refine AND smooth toward the limit surface (rounds low-poly cages). Boundary edges
// stay on the boundary curve; interior points use the standard Loop weights.
inline SubdivMesh subdivideMeshLoop(const std::vector<math::vec3>& positions,
                                   const std::vector<std::uint32_t>& indices, int iterations = 1) {
    SubdivMesh m{positions, indices};
    for (int i = 0; i < iterations && !m.indices.empty(); ++i)
        m = detail::subdivideOnce(m.positions, m.indices, true);
    return m;
}

} // namespace maz::render
