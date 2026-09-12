#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "maz/math/Math.hpp"

// maz::render vertex welding — merge coincident (or near-coincident) vertices of an indexed triangle
// mesh into one, remapping the indices and dropping triangles that collapse to a line. It is the cleanup
// pass a mesh needs after being built face-by-face (each quad or triangle emitting its own corners), CSG
// or marching-cubes output, or an import that duplicated shared vertices along every seam: welding turns
// that soft soup into a compact shared-vertex mesh so smoothing groups, subdivision, and normal
// generation behave (a shared corner must be ONE vertex for its faces to average). This is Godot's
// SurfaceTool.index() with a distance threshold. A spatial hash keyed by the weld cell finds candidates
// in O(n) expected; the first occurrence of each cluster is kept as the representative, so surviving
// positions are unchanged. Godot's SurfaceTool.index welds only bit-exact duplicates; the distance
// threshold here is the extra step -> parity-or-better. Header-only, std-only, deterministic.
namespace maz::render {

struct WeldedMesh {
    std::vector<math::vec3> positions;
    std::vector<std::uint32_t> indices;
    std::uint32_t removedTriangles = 0; // triangles dropped because two corners merged
};

namespace detail {

// Hash of an integer weld-cell coordinate. Collisions only add distance-checked candidates, never lose
// them, so the weld stays correct regardless of hash quality.
inline std::uint64_t cellHash(long cx, long cy, long cz) {
    std::uint64_t h = 1469598103934665603ULL; // FNV offset
    auto mix = [&h](long v) {
        h ^= static_cast<std::uint64_t>(v) * 0x9E3779B97F4A7C15ULL;
        h *= 1099511628211ULL;
    };
    mix(cx); mix(cy); mix(cz);
    return h;
}

} // namespace detail

// Weld vertices closer than `epsilon` in each axis' cell. epsilon <= 0 welds only bit-exact duplicates.
inline WeldedMesh weldVertices(const std::vector<math::vec3>& positions,
                              const std::vector<std::uint32_t>& indices, float epsilon = 1e-5f) {
    using math::vec3;
    WeldedMesh out;
    if (positions.empty()) return out;

    const float cell = epsilon > 0.0f ? epsilon : 1e-9f;
    const float eps2 = epsilon > 0.0f ? epsilon * epsilon : 0.0f;

    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid;
    grid.reserve(positions.size());
    std::vector<std::uint32_t> remap(positions.size()); // old index -> new (kept) index

    auto cellOf = [&](float v) { return static_cast<long>(std::floor(static_cast<double>(v) / cell)); };

    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(positions.size()); ++i) {
        const vec3 p = positions[i];
        const long cx = cellOf(p.x), cy = cellOf(p.y), cz = cellOf(p.z);
        std::uint32_t found = 0xFFFFFFFFu;
        // Search the 3x3x3 neighbourhood so points either side of a cell boundary still merge.
        for (long dz = -1; dz <= 1 && found == 0xFFFFFFFFu; ++dz)
            for (long dy = -1; dy <= 1 && found == 0xFFFFFFFFu; ++dy)
                for (long dx = -1; dx <= 1 && found == 0xFFFFFFFFu; ++dx) {
                    const auto it = grid.find(detail::cellHash(cx + dx, cy + dy, cz + dz));
                    if (it == grid.end()) continue;
                    for (std::uint32_t kept : it->second) {
                        const vec3 d = out.positions[kept] - p;
                        const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
                        if (dist2 <= eps2) { found = kept; break; }
                    }
                }
        if (found != 0xFFFFFFFFu) {
            remap[i] = found;
        } else {
            const std::uint32_t newIndex = static_cast<std::uint32_t>(out.positions.size());
            out.positions.push_back(p);
            grid[detail::cellHash(cx, cy, cz)].push_back(newIndex);
            remap[i] = newIndex;
        }
    }

    // Remap the index buffer, dropping triangles that collapsed to a line (two shared corners).
    const std::size_t triCount = indices.size() / 3;
    out.indices.reserve(indices.size());
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t a = remap[indices[t * 3 + 0]];
        const std::uint32_t b = remap[indices[t * 3 + 1]];
        const std::uint32_t c = remap[indices[t * 3 + 2]];
        if (a == b || b == c || a == c) {
            ++out.removedTriangles;
            continue;
        }
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    return out;
}

} // namespace maz::render
