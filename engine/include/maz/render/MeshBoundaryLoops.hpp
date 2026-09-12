#pragma once

#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology
#include "maz/render/Shapes.hpp"       // shapes::MeshData

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::render BOUNDARY / HOLE edge-loop extraction — walk the open edges of a mesh into the ordered vertex
// LOOPS that ring each hole or the outer rim of an open surface. MeshTopology (M528) already counts boundary
// edges; this turns them into usable curves, the front end for hole FILLING (cap each loop with a fan/
// triangulation), silhouette/outline rendering, cloth/rope attachment along an edge, and "select boundary" in
// an editor. Each boundary undirected edge is used by exactly one triangle, so its single directed half-edge
// (wound by that triangle) points consistently around the hole; chaining next[a]=b from those half-edges
// yields the loops. Pure CPU, header-only, headless.
//
// Scope note (honest): the walk is per-HALF-EDGE and any chain that revisits a vertex is split there, so a
// PINCH corner — where two holes meet at a single vertex, as when two triangles sharing only that corner are
// deleted — yields both rims. (Keying the chain by vertex instead, as this did before, let the second hole's
// edge overwrite the first's: a whole loop vanished from the result and from analyzeWatertight's holeCount, so
// a caller patching every reported hole still had an open mesh.) Edges of a NON-manifold (3+ face) edge are
// twinless in MeshTopology and so look open here; they are walked like boundary edges, bounded, never spun.
namespace maz::render {

// Split a walked boundary chain wherever it visits a vertex twice. The rim of a single hole never repeats a
// vertex, so a repeat means the walk passed through a PINCH corner — a vertex where two holes meet — and
// chained two rims into one figure-eight. Which of the two pairings a fan walk produces at such a corner
// depends on the geometry, not on connectivity, so neither direction is right everywhere; splitting the chain
// afterwards recovers the separate rims either way, and leaves an ordinary loop untouched.
inline void splitAtPinches(std::vector<std::uint32_t> chain,
                           std::vector<std::vector<std::uint32_t>>& out) {
    for (;;) {
        std::unordered_map<std::uint32_t, std::size_t> firstAt;
        firstAt.reserve(chain.size());
        std::size_t i = 0, j = 0;
        bool repeated = false;
        for (std::size_t k = 0; k < chain.size(); ++k) {
            const auto it = firstAt.find(chain[k]);
            if (it != firstAt.end()) {
                i = it->second;
                j = k;
                repeated = true;
                break;
            }
            firstAt.emplace(chain[k], k);
        }
        if (!repeated) {
            if (chain.size() >= 3) out.push_back(std::move(chain));
            return;
        }
        const auto b = chain.begin() + static_cast<std::ptrdiff_t>(i);
        const auto e = chain.begin() + static_cast<std::ptrdiff_t>(j);
        std::vector<std::uint32_t> sub(b, e);
        chain.erase(b, e);
        if (sub.size() >= 3) out.push_back(std::move(sub));
    }
}

// Extract the boundary loops of `mesh`. Each loop is an ordered list of vertex indices ringing one hole /
// open rim (not repeating the first vertex at the end). A watertight mesh yields no loops. Loops are found in
// order of first-seen boundary vertex, so the result is deterministic.
inline std::vector<std::vector<std::uint32_t>> extractBoundaryLoops(const shapes::MeshData& mesh) {
    std::vector<std::vector<std::uint32_t>> loops;
    const MeshTopology topo = buildTopology(mesh);
    if (topo.triangleCount == 0) return loops;

    const std::uint32_t heCount = static_cast<std::uint32_t>(topo.opposite.size());
    auto nextInTri = [](std::uint32_t he) { return he - he % 3 + (he % 3 + 1) % 3; };
    auto fromVertex = [&](std::uint32_t he) { return mesh.indices[he]; };
    auto isOpen = [&](std::uint32_t he) { return topo.opposite[he] == MeshTopology::kNone; };

    std::vector<std::uint8_t> used(heCount, 0);
    // Start candidates in ascending start-vertex order, so loops still come out in order of first-seen
    // boundary vertex.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> starts; // (start vertex, half-edge)
    starts.reserve(heCount);
    for (std::uint32_t he = 0; he < heCount; ++he) {
        if (isOpen(he)) starts.emplace_back(fromVertex(he), he);
    }
    std::sort(starts.begin(), starts.end());

    for (const auto& s : starts) {
        if (used[s.second]) continue;
        std::vector<std::uint32_t> chain;
        std::uint32_t he = s.second;
        while (!used[he]) {
            used[he] = 1;
            chain.push_back(fromVertex(he));
            // Rotate around this half-edge's END vertex through its triangle fan to the next open edge
            // leaving it. One open edge in, one out: every boundary edge is walked exactly once.
            std::uint32_t h = nextInTri(he);
            std::uint32_t guard = 0;
            while (!isOpen(h) && guard++ < heCount) h = nextInTri(topo.opposite[h]);
            if (!isOpen(h)) break; // malformed fan: bail rather than spin
            he = h;
        }
        splitAtPinches(std::move(chain), loops);
    }
    return loops;
}

} // namespace maz::render
