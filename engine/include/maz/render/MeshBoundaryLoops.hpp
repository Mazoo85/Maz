#pragma once

#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology
#include "maz/render/Shapes.hpp"       // shapes::MeshData

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render BOUNDARY / HOLE edge-loop extraction — walk the open edges of a mesh into the ordered vertex
// LOOPS that ring each hole or the outer rim of an open surface. MeshTopology (M528) already counts boundary
// edges; this turns them into usable curves, the front end for hole FILLING (cap each loop with a fan/
// triangulation), silhouette/outline rendering, cloth/rope attachment along an edge, and "select boundary" in
// an editor. Each boundary undirected edge is used by exactly one triangle, so its single directed half-edge
// (wound by that triangle) points consistently around the hole; chaining next[a]=b from those half-edges
// yields the loops. Pure CPU, header-only, headless.
//
// Scope note (honest): manifold boundaries (each boundary vertex on exactly one loop). A non-manifold
// figure-eight boundary vertex is resolved arbitrarily (last edge wins) — the documented edge case.
namespace maz::render {

// Extract the boundary loops of `mesh`. Each loop is an ordered list of vertex indices ringing one hole /
// open rim (not repeating the first vertex at the end). A watertight mesh yields no loops. Loops are found in
// order of first-seen boundary vertex, so the result is deterministic.
inline std::vector<std::vector<std::uint32_t>> extractBoundaryLoops(const shapes::MeshData& mesh) {
    std::vector<std::vector<std::uint32_t>> loops;
    const MeshTopology topo = buildTopology(mesh);
    if (topo.triangleCount == 0) return loops;

    // Directed boundary half-edge a->b (opposite twin is kNone => the undirected edge has one triangle).
    std::unordered_map<std::uint32_t, std::uint32_t> next;
    next.reserve(topo.opposite.size());
    for (std::uint32_t he = 0; he < topo.opposite.size(); ++he) {
        if (topo.opposite[he] != MeshTopology::kNone) continue;
        const std::uint32_t t = he / 3, e = he % 3;
        const std::uint32_t a = mesh.indices[static_cast<std::size_t>(t) * 3 + e];
        const std::uint32_t b = mesh.indices[static_cast<std::size_t>(t) * 3 + (e + 1) % 3];
        next[a] = b;
    }

    std::vector<std::uint8_t> used(mesh.vertices.size(), 0);
    // Walk in ascending start-vertex order for determinism.
    std::vector<std::uint32_t> starts;
    starts.reserve(next.size());
    for (const auto& kv : next) starts.push_back(kv.first);
    std::sort(starts.begin(), starts.end());

    for (std::uint32_t start : starts) {
        if (start >= used.size() || used[start]) continue;
        std::vector<std::uint32_t> loop;
        std::uint32_t v = start;
        // Follow the chain until it returns to the start (or breaks on a malformed boundary).
        while (v < used.size() && !used[v]) {
            used[v] = 1;
            loop.push_back(v);
            auto it = next.find(v);
            if (it == next.end()) break; // open chain (shouldn't happen on a manifold boundary)
            v = it->second;
        }
        if (loop.size() >= 3) loops.push_back(std::move(loop));
    }
    return loops;
}

} // namespace maz::render
