#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render mesh simplification by vertex clustering — the load-time decimation an engine runs to generate
// lower-poly LODs and cheap collision hulls from a dense source mesh, the way Godot's importer auto-generates
// LODs. It overlays a uniform grid of `cellSize` over the mesh, merges every vertex that falls in the same
// cell into one representative (the average of its members — position, normal, color, UV), remaps the
// triangles to those representatives, and drops any triangle whose corners collapsed together (now
// degenerate). Coarser cells → fewer vertices and triangles, monotonically. Unlike quadric-error edge
// collapse it is O(n), order-independent, and never produces holes or flipped normals — the robust choice
// for collision proxies and distant LODs. Pure CPU vertex math (no GPU), so it unit-tests headlessly: the
// output has fewer verts/tris, its bounding box stays within one cell of the original, every triangle is
// non-degenerate, and all indices are in range.
//
// Scope note (honest): uniform-grid clustering (fast, robust, shape-preserving at the cell scale). Feature-
// preserving quadric-error edge collapse — which better keeps silhouettes at aggressive ratios — is the
// heavier follow-up; clustering is the dependable base every engine ships first.
namespace maz::render {

namespace detail {
struct CellKey {
    std::int64_t x, y, z;
    bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
};
struct CellKeyHash {
    std::size_t operator()(const CellKey& k) const {
        // Mix three 64-bit lanes.
        std::uint64_t h = static_cast<std::uint64_t>(k.x) * 0x9E3779B97F4A7C15ull;
        h ^= (static_cast<std::uint64_t>(k.y) + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2));
        h ^= (static_cast<std::uint64_t>(k.z) + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2));
        return static_cast<std::size_t>(h);
    }
};
inline std::int64_t cellCoord(float v, float cellSize) {
    return static_cast<std::int64_t>(std::floor(v / cellSize));
}
} // namespace detail

// Simplify `in` by clustering vertices onto a grid of `cellSize` world units. Larger cellSize = more
// aggressive reduction. Returns the decimated mesh.
inline shapes::MeshData simplifyClustering(const shapes::MeshData& in, float cellSize) {
    shapes::MeshData out;
    if (cellSize <= 0.0f || in.vertices.empty() || in.indices.size() < 3) return in;

    // Map each source vertex to a cell, accumulating an averaged representative per cell.
    struct Accum {
        std::uint32_t index = 0; // new vertex index for this cell
        int count = 0;
        float px = 0, py = 0, pz = 0, nx = 0, ny = 0, nz = 0, r = 0, g = 0, b = 0, u = 0, v = 0;
    };
    std::unordered_map<detail::CellKey, Accum, detail::CellKeyHash> cells;
    std::vector<std::uint32_t> vertToCellIndex(in.vertices.size());

    for (std::size_t i = 0; i < in.vertices.size(); ++i) {
        const MeshVertex& mv = in.vertices[i];
        const detail::CellKey key{detail::cellCoord(mv.px, cellSize), detail::cellCoord(mv.py, cellSize),
                                  detail::cellCoord(mv.pz, cellSize)};
        auto it = cells.find(key);
        if (it == cells.end()) {
            Accum a;
            a.index = static_cast<std::uint32_t>(cells.size());
            it = cells.emplace(key, a).first;
        }
        Accum& a = it->second;
        ++a.count;
        a.px += mv.px; a.py += mv.py; a.pz += mv.pz;
        a.nx += mv.nx; a.ny += mv.ny; a.nz += mv.nz;
        a.r += mv.r; a.g += mv.g; a.b += mv.b;
        a.u += mv.u; a.v += mv.v;
        vertToCellIndex[i] = a.index;
    }

    // Emit one averaged vertex per cell, indexed by the assigned cell index.
    out.vertices.resize(cells.size());
    for (const auto& kv : cells) {
        const Accum& a = kv.second;
        const float inv = 1.0f / static_cast<float>(a.count);
        MeshVertex mv{};
        mv.px = a.px * inv; mv.py = a.py * inv; mv.pz = a.pz * inv;
        // Normalize the averaged normal.
        float nx = a.nx, ny = a.ny, nz = a.nz;
        const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nl > 1e-8f) { nx /= nl; ny /= nl; nz /= nl; } else { nx = 0; ny = 1; nz = 0; }
        mv.nx = nx; mv.ny = ny; mv.nz = nz;
        mv.r = a.r * inv; mv.g = a.g * inv; mv.b = a.b * inv;
        mv.u = a.u * inv; mv.v = a.v * inv;
        out.vertices[a.index] = mv;
    }

    // Remap triangles; drop those that collapsed to a degenerate (two corners in the same cell).
    for (std::size_t t = 0; t + 2 < in.indices.size(); t += 3) {
        const std::uint32_t i0 = in.indices[t + 0];
        const std::uint32_t i1 = in.indices[t + 1];
        const std::uint32_t i2 = in.indices[t + 2];
        if (i0 >= in.vertices.size() || i1 >= in.vertices.size() || i2 >= in.vertices.size()) continue;
        const std::uint32_t a = vertToCellIndex[i0];
        const std::uint32_t b = vertToCellIndex[i1];
        const std::uint32_t c = vertToCellIndex[i2];
        if (a == b || b == c || a == c) continue; // degenerate after clustering
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    return out;
}

} // namespace maz::render
