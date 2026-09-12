#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

// maz::render WIREFRAME / EDGE EXTRACTION — pull the UNIQUE edges out of a triangle mesh so you can draw it as a
// cage of lines. Every triangle shares its edges with its neighbours, so the raw triangle list mentions each interior
// edge twice; this collapses them to one edge each. Feed the result to a line renderer (drawLine) for a wireframe
// overlay, an editor "show edges" mode, a hologram / blueprint look, a selection highlight outline, or a debug view
// of how a mesh is built. `meshEdges` returns the edges as vertex-index PAIRS; `meshWireframe` returns them ready to
// draw as a flat LINE LIST — two positions per edge, so you can hand the whole vector straight to a line-segment
// draw call. Header-only, deterministic, headless.
//
// Scope note (honest): this returns EVERY triangle edge (deduplicated) — including the diagonal that splits each
// quad face into two triangles, so a triangulated cube yields 18 edges (12 box edges + 6 face diagonals), not the 12
// "clean" edges an artist sees. For only the visually meaningful creases, use the sharp / hard / feature-edge tools
// (M548/M549/M567) which filter by dihedral angle. Edges are undirected and de-duplicated; a degenerate index in a
// triangle is skipped.
namespace maz::render {

// The unique undirected edges of `mesh`, as (loIndex, hiIndex) vertex-index pairs.
inline std::vector<std::pair<std::uint32_t, std::uint32_t>> meshEdges(const shapes::MeshData& mesh) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0) return out;
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(mesh.indices.size());
    auto add = [&](std::uint32_t u, std::uint32_t v) {
        if (u == v) return;
        const std::uint32_t lo = u < v ? u : v, hi = u < v ? v : u;
        const std::uint64_t key = (static_cast<std::uint64_t>(lo) << 32) | hi;
        if (seen.insert(key).second) out.emplace_back(lo, hi);
    };
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t a = mesh.indices[t * 3 + 0];
        const std::uint32_t b = mesh.indices[t * 3 + 1];
        const std::uint32_t c = mesh.indices[t * 3 + 2];
        if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) continue;
        add(a, b); add(b, c); add(c, a);
    }
    return out;
}

// The mesh's unique edges as a flat LINE LIST: two positions per edge (segment i is [2i], [2i+1]).
inline std::vector<math::vec3> meshWireframe(const shapes::MeshData& mesh) {
    std::vector<math::vec3> segs;
    const auto edges = meshEdges(mesh);
    segs.reserve(edges.size() * 2);
    for (const auto& e : edges) {
        const MeshVertex& a = mesh.vertices[e.first];
        const MeshVertex& b = mesh.vertices[e.second];
        segs.push_back(math::vec3(a.px, a.py, a.pz));
        segs.push_back(math::vec3(b.px, b.py, b.pz));
    }
    return segs;
}

} // namespace maz::render
