#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

// maz::render INDEX / DEDUPLICATE — turn a "triangle soup" (a mesh where every triangle carries its own three
// corners, so shared corners are stored two, three or six times over) into a compact INDEXED mesh: keep one copy
// of each truly-identical vertex and point every triangle at it through a fresh index buffer. This is the cleanup
// most of the engine's own shape builders, CSG output, marching-cubes/SurfaceNets output, and flat OBJ/STL
// imports need — they emit unshared corners, which bloats the vertex buffer and stops the GPU's post-transform
// vertex cache from ever hitting. Godot's SurfaceTool.index() is exactly this.
//
// The key difference from M-weld (position welding): this merges vertices ONLY when EVERY attribute matches bit
// for bit — position AND normal AND colour AND UV. That is deliberately conservative: two corners that sit at the
// same point but carry different normals (a hard crease) or different UVs (a texture seam) are LEFT SEPARATE,
// because collapsing them would smooth the crease or tear the texture. Position welding (which ignores normals and
// UVs) is the tool when you WANT to fuse a seam; reindexing is the tool when you want a smaller buffer with the
// look completely unchanged. Degenerate triangles (two corners that were already the same vertex) are dropped.
// Header-only, std-only, deterministic — the first occurrence of each unique vertex keeps its slot, so output
// order is stable.
namespace maz::render {

struct ReindexReport {
    shapes::MeshData mesh;                  // the compacted, indexed mesh
    std::uint32_t uniqueVertices = 0;       // vertices in the output (== mesh.vertices.size())
    std::uint32_t mergedVertices = 0;       // input vertices that collapsed onto an earlier identical one
    std::uint32_t removedTriangles = 0;     // triangles dropped because two corners were the same vertex
};

namespace detail {

// A bit-exact key over all eleven floats of a MeshVertex. Uses the raw bit pattern (via memcpy, so -0.0f and NaN
// compare by bits, never by value) so "identical" means byte-for-byte identical.
struct VertexKey {
    std::uint32_t bits[11];
    bool operator==(const VertexKey& o) const {
        for (int i = 0; i < 11; ++i)
            if (bits[i] != o.bits[i]) return false;
        return true;
    }
};

inline VertexKey keyOf(const MeshVertex& v) {
    const float f[11] = {v.px, v.py, v.pz, v.nx, v.ny, v.nz, v.r, v.g, v.b, v.u, v.v};
    VertexKey k;
    std::memcpy(k.bits, f, sizeof(k.bits));
    return k;
}

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& k) const {
        std::uint64_t h = 1469598103934665603ULL; // FNV-1a
        for (int i = 0; i < 11; ++i) {
            h ^= k.bits[i];
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }
};

} // namespace detail

// Deduplicate identical vertices and rebuild the index buffer. If `mesh.indices` is empty the vertices are treated
// as an implicit triangle soup (0,1,2, 3,4,5, ...). Vertices merge only when all 11 attributes match bit-for-bit,
// so creases and UV seams survive. Degenerate triangles are dropped.
inline ReindexReport reindexMesh(const shapes::MeshData& mesh) {
    ReindexReport out;
    const std::size_t vin = mesh.vertices.size();
    if (vin == 0) return out;

    // Source index stream: the given indices, or the implicit sequence if none were supplied.
    const bool implicit = mesh.indices.empty();
    const std::size_t indexCount = implicit ? vin : mesh.indices.size();

    std::unordered_map<detail::VertexKey, std::uint32_t, detail::VertexKeyHash> seen;
    seen.reserve(vin);
    std::vector<std::uint32_t> remap(vin, 0xFFFFFFFFu); // old vertex index -> new (kept) index, lazily filled

    auto keep = [&](std::uint32_t oldIndex) -> std::uint32_t {
        std::uint32_t& slot = remap[oldIndex];
        if (slot != 0xFFFFFFFFu) return slot; // already resolved this source vertex
        const detail::VertexKey k = detail::keyOf(mesh.vertices[oldIndex]);
        const auto it = seen.find(k);
        if (it != seen.end()) {
            slot = it->second;
            ++out.mergedVertices;
            return slot;
        }
        const std::uint32_t newIndex = static_cast<std::uint32_t>(out.mesh.vertices.size());
        out.mesh.vertices.push_back(mesh.vertices[oldIndex]);
        seen.emplace(k, newIndex);
        slot = newIndex;
        return newIndex;
    };

    out.mesh.indices.reserve(indexCount);
    const std::size_t triCount = indexCount / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        std::uint32_t src[3];
        for (int c = 0; c < 3; ++c) {
            const std::uint32_t raw = implicit ? static_cast<std::uint32_t>(t * 3 + static_cast<std::size_t>(c))
                                               : mesh.indices[t * 3 + static_cast<std::size_t>(c)];
            src[c] = keep(raw);
        }
        if (src[0] == src[1] || src[1] == src[2] || src[0] == src[2]) {
            ++out.removedTriangles;
            continue;
        }
        out.mesh.indices.push_back(src[0]);
        out.mesh.indices.push_back(src[1]);
        out.mesh.indices.push_back(src[2]);
    }

    out.uniqueVertices = static_cast<std::uint32_t>(out.mesh.vertices.size());
    return out;
}

} // namespace maz::render
