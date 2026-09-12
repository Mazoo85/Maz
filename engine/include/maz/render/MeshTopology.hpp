#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render MESH TOPOLOGY / connectivity — the reusable half-edge-style adjacency an engine builds once and
// then everything that needs to know how a mesh is STITCHED TOGETHER queries: boundary/hole detection,
// watertightness (is this a closed solid?), per-triangle neighbours for flood-fill smoothing groups / UV
// islands / crease detection, and the Euler characteristic as a sanity check. Godot exposes the same thing
// via MeshDataTool (get_edge_faces, get_face_edges). Existing modules built ad-hoc edge maps inline
// (MeshSmooth's vertex-vertex adjacency, Subdivision's EdgeInfo); this is the shared, tested primitive.
//
// The mesh is a triangle list. Each triangle owns three DIRECTED half-edges: for triangle t,
// he 3t+0 = v0→v1, 3t+1 = v1→v2, 3t+2 = v2→v0. Two half-edges are TWINS when they share the same undirected
// edge {a,b}; `opposite[he]` is that twin (or kNone on a boundary / non-manifold edge). Twin matching is by
// UNDIRECTED edge, so it is robust to inconsistent winding (a mis-wound but connected mesh still reports its
// neighbours, not spurious boundaries). Pure CPU, header-only, headless.
namespace maz::render {

struct MeshTopology {
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;

    std::uint32_t vertexCount = 0;
    std::uint32_t triangleCount = 0;
    std::uint32_t edgeCount = 0;           // distinct undirected edges
    std::uint32_t boundaryEdgeCount = 0;   // undirected edges used by exactly one triangle (holes / open rim)
    std::uint32_t nonManifoldEdgeCount = 0;// undirected edges shared by three+ triangles

    std::vector<std::uint32_t> opposite;   // per half-edge (size 3*triangleCount): twin half-edge or kNone
    std::vector<std::uint8_t>  boundaryVertex; // per vertex: 1 if it touches a boundary edge

    // A closed, edge-manifold surface: no holes and no edge shared by 3+ faces.
    bool watertight() const { return boundaryEdgeCount == 0 && nonManifoldEdgeCount == 0; }

    // Neighbouring triangle across local edge `edge` (0..2) of triangle `t`, or kNone if that edge is a
    // boundary / non-manifold edge.
    std::uint32_t triangleNeighbor(std::uint32_t t, int edge) const {
        const std::uint32_t he = opposite[static_cast<std::size_t>(t) * 3 + static_cast<std::uint32_t>(edge)];
        return he == kNone ? kNone : he / 3;
    }

    // V - E + F. 2 for a closed genus-0 solid (a sphere/cube), 1 for a single-boundary disk.
    int eulerCharacteristic() const {
        return static_cast<int>(vertexCount) - static_cast<int>(edgeCount) + static_cast<int>(triangleCount);
    }
};

// Build the topology of a triangle list. `vertexCount` bounds the boundary-vertex table; indices beyond it
// are ignored defensively.
inline MeshTopology buildTopology(std::uint32_t vertexCount, const std::vector<std::uint32_t>& indices) {
    MeshTopology topo;
    topo.vertexCount = vertexCount;
    topo.triangleCount = static_cast<std::uint32_t>(indices.size() / 3);
    topo.opposite.assign(static_cast<std::size_t>(topo.triangleCount) * 3, MeshTopology::kNone);
    topo.boundaryVertex.assign(vertexCount, 0);

    auto ukey = [](std::uint32_t a, std::uint32_t b) -> std::uint64_t {
        const std::uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
        return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
    };

    // Group half-edges by their undirected edge.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> edges;
    edges.reserve(topo.opposite.size());
    for (std::uint32_t t = 0; t < topo.triangleCount; ++t) {
        const std::uint32_t i0 = indices[static_cast<std::size_t>(t) * 3];
        const std::uint32_t i1 = indices[static_cast<std::size_t>(t) * 3 + 1];
        const std::uint32_t i2 = indices[static_cast<std::size_t>(t) * 3 + 2];
        const std::uint32_t tri[3] = {i0, i1, i2};
        for (std::uint32_t e = 0; e < 3; ++e) {
            const std::uint32_t a = tri[e], b = tri[(e + 1) % 3];
            edges[ukey(a, b)].push_back(t * 3 + e);
        }
    }

    topo.edgeCount = static_cast<std::uint32_t>(edges.size());
    for (const auto& kv : edges) {
        const std::vector<std::uint32_t>& hes = kv.second;
        if (hes.size() == 2) {
            topo.opposite[hes[0]] = hes[1];
            topo.opposite[hes[1]] = hes[0];
        } else if (hes.size() == 1) {
            ++topo.boundaryEdgeCount;
            const std::uint32_t lo = static_cast<std::uint32_t>(kv.first >> 32);
            const std::uint32_t hi = static_cast<std::uint32_t>(kv.first & 0xFFFFFFFFu);
            if (lo < vertexCount) topo.boundaryVertex[lo] = 1;
            if (hi < vertexCount) topo.boundaryVertex[hi] = 1;
        } else {
            ++topo.nonManifoldEdgeCount; // 3+ faces on one edge: leave those half-edges twinless
        }
    }
    return topo;
}

inline MeshTopology buildTopology(const shapes::MeshData& mesh) {
    return buildTopology(static_cast<std::uint32_t>(mesh.vertices.size()), mesh.indices);
}

} // namespace maz::render
