#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <cstring>       // std::memcmp
#include <unordered_map>
#include <vector>

// maz::render MESH CLEANUP — the import/optimization hygiene pass that shrinks a mesh without changing what it
// draws: merge BIT-EXACT duplicate vertices (identical in EVERY attribute — position, normal, colour, UV) into
// one, drop DEGENERATE triangles (two corners the same index -> zero area), and remove UNUSED vertices (no
// surviving triangle references them), compacting the buffers. Importers and mesh generators routinely emit
// bloat — a glTF/OBJ authored face-by-face repeats every shared corner; CSG/boolean and marching-cubes output
// leave orphaned vertices; edits leave slivers — and that bloat costs VRAM, breaks the vertex cache, and
// stops smoothing/subdivision from treating a shared corner as one point. This is Godot's SurfaceTool.index()
// hygiene, but attribute-exact and attribute-preserving (unlike MeshWeld, which welds by SPATIAL proximity and
// keeps positions only): use MeshWeld to fuse near-coincident corners after generation, use cleanupMesh to
// strip exact redundancy while keeping normals/UVs intact. Deterministic (survivors keep first-seen order),
// header-only, std-only.
namespace maz::render {

struct MeshCleanupStats {
    std::size_t verticesBefore = 0;
    std::size_t verticesAfter = 0;
    std::size_t trianglesBefore = 0;
    std::size_t trianglesAfter = 0;
    std::size_t duplicatesMerged = 0;   // vertices folded into an earlier byte-identical vertex
    std::size_t unusedRemoved = 0;      // representative vertices no surviving triangle referenced
    std::size_t degenerateRemoved = 0;  // triangles dropped for having a repeated corner index
};

// Return a cleaned copy of `mesh`: exact-duplicate vertices merged, unused vertices dropped, degenerate
// triangles removed, buffers compacted. Rendered geometry is unchanged (same triangles, same winding, same
// attributes). Pass `stats` to receive the before/after counts. An empty mesh yields an empty mesh.
inline shapes::MeshData cleanupMesh(const shapes::MeshData& mesh, MeshCleanupStats* stats = nullptr) {
    constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    const std::vector<MeshVertex>& V = mesh.vertices;

    MeshCleanupStats st;
    st.verticesBefore = V.size();
    st.trianglesBefore = mesh.indices.size() / 3;

    // 1. Fold byte-identical vertices onto their first occurrence (FNV-1a hash buckets, exact memcmp confirm).
    auto hashVertex = [](const MeshVertex& v) {
        const auto* p = reinterpret_cast<const unsigned char*>(&v);
        std::uint64_t h = 1469598103934665603ULL;
        for (std::size_t k = 0; k < sizeof(MeshVertex); ++k) {
            h ^= static_cast<std::uint64_t>(p[k]);
            h *= 1099511628211ULL;
        }
        return h;
    };
    std::vector<std::uint32_t> rep(V.size(), kNone); // old index -> representative old index
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> buckets;
    buckets.reserve(V.size());
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(V.size()); ++i) {
        const std::uint64_t h = hashVertex(V[i]);
        std::uint32_t found = kNone;
        auto& bucket = buckets[h];
        for (std::uint32_t cand : bucket)
            if (std::memcmp(&V[cand], &V[i], sizeof(MeshVertex)) == 0) { found = cand; break; }
        if (found != kNone) { rep[i] = found; ++st.duplicatesMerged; }
        else { rep[i] = i; bucket.push_back(i); }
    }

    // 2. Remap triangles onto representatives, dropping any that collapse to a repeated corner.
    std::vector<std::uint32_t> remapped;
    remapped.reserve(mesh.indices.size());
    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t a = rep[mesh.indices[t * 3 + 0]];
        const std::uint32_t b = rep[mesh.indices[t * 3 + 1]];
        const std::uint32_t c = rep[mesh.indices[t * 3 + 2]];
        if (a == b || b == c || a == c) { ++st.degenerateRemoved; continue; }
        remapped.push_back(a);
        remapped.push_back(b);
        remapped.push_back(c);
    }

    // 3. Compact: emit each used representative once, in first-referenced order; reindex.
    shapes::MeshData out;
    std::vector<std::uint32_t> newIndex(V.size(), kNone);
    out.indices.reserve(remapped.size());
    for (std::uint32_t oldIdx : remapped) {
        if (newIndex[oldIdx] == kNone) {
            newIndex[oldIdx] = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(V[oldIdx]);
        }
        out.indices.push_back(newIndex[oldIdx]);
    }

    st.verticesAfter = out.vertices.size();
    st.trianglesAfter = out.indices.size() / 3;
    // before = after (used reps) + merged (duplicates) + unused (reps referenced by no surviving triangle)
    st.unusedRemoved = st.verticesBefore - st.duplicatesMerged - st.verticesAfter;
    if (stats) *stats = st;
    return out;
}

} // namespace maz::render
