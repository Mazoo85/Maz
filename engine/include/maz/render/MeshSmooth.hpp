#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "maz/math/Math.hpp"

// maz::render Laplacian / Taubin mesh smoothing — relax the vertices of an indexed triangle mesh toward
// the average of their neighbours, ironing out noise and faceting WITHOUT changing the topology (no
// vertices added or removed, only moved). It is the denoise pass for a mesh built from noisy data: a
// marching-cubes isosurface, a heightfield perturbed by fbm, a scanned/voxelised blob. Plain Laplacian
// smoothing shrinks the shape (every point drifts inward); the Taubin lambda|mu two-pass alternates a
// positive (smoothing) and a slightly larger negative (unshrinking) step so the surface relaxes while
// its volume is preserved — the standard low-pass mesh filter. Boundary vertices can be pinned so open
// edges keep their shape. Reuses the engine's positions+indices mesh form (MeshTools / subdivision /
// weld family). Godot exposes no runtime mesh smoothing to gameplay code, so this is a beyond-Godot
// geometry utility. Header-only, std-only, deterministic.
namespace maz::render {

namespace detail {

// Build, for every vertex, the set of vertices it shares an edge with, plus a boundary flag.
inline void buildAdjacency(std::size_t vertexCount, const std::vector<std::uint32_t>& indices,
                           std::vector<std::vector<std::uint32_t>>& neighbours,
                           std::vector<bool>& boundary) {
    neighbours.assign(vertexCount, {});
    boundary.assign(vertexCount, false);
    // Count how many triangles use each undirected edge; edges used once are boundary edges.
    std::unordered_map<std::uint64_t, int> edgeCount;
    const std::size_t triCount = indices.size() / 3;
    auto key = [](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
        return (static_cast<std::uint64_t>(lo) << 32) | hi;
    };
    std::vector<std::unordered_set<std::uint32_t>> sets(vertexCount);
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t v[3] = {indices[t * 3], indices[t * 3 + 1], indices[t * 3 + 2]};
        for (int e = 0; e < 3; ++e) {
            const std::uint32_t a = v[e], b = v[static_cast<std::size_t>((e + 1) % 3)];
            sets[a].insert(b);
            sets[b].insert(a);
            ++edgeCount[key(a, b)];
        }
    }
    for (const auto& [k, c] : edgeCount) {
        if (c == 1) {
            boundary[static_cast<std::size_t>(k >> 32)] = true;
            boundary[static_cast<std::size_t>(k & 0xffffffffu)] = true;
        }
    }
    for (std::size_t i = 0; i < vertexCount; ++i)
        neighbours[i].assign(sets[i].begin(), sets[i].end());
}

// One weighted-Laplacian pass: move each interior vertex a fraction `factor` toward its neighbour
// centroid. Boundary vertices are left in place when pinBoundary is set.
inline void laplacianPass(std::vector<math::vec3>& pos,
                          const std::vector<std::vector<std::uint32_t>>& nbr,
                          const std::vector<bool>& boundary, float factor, bool pinBoundary) {
    using math::vec3;
    std::vector<vec3> next = pos;
    for (std::size_t i = 0; i < pos.size(); ++i) {
        if (nbr[i].empty()) continue;
        if (pinBoundary && boundary[i]) continue;
        vec3 sum(0.0f);
        for (std::uint32_t j : nbr[i]) sum += pos[j];
        const vec3 centroid = sum / static_cast<float>(nbr[i].size());
        next[i] = pos[i] + (centroid - pos[i]) * factor;
    }
    pos.swap(next);
}

} // namespace detail

// Plain Laplacian smoothing: `iterations` passes each moving vertices `lambda` (0..1) toward their
// neighbour centroid. Simple and strong, but shrinks the shape; use Taubin to preserve volume.
inline std::vector<math::vec3> smoothMeshLaplacian(const std::vector<math::vec3>& positions,
                                                  const std::vector<std::uint32_t>& indices,
                                                  int iterations = 1, float lambda = 0.5f,
                                                  bool pinBoundary = true) {
    std::vector<math::vec3> pos = positions;
    if (pos.empty()) return pos;
    std::vector<std::vector<std::uint32_t>> nbr;
    std::vector<bool> boundary;
    detail::buildAdjacency(pos.size(), indices, nbr, boundary);
    for (int i = 0; i < iterations; ++i) detail::laplacianPass(pos, nbr, boundary, lambda, pinBoundary);
    return pos;
}

// Taubin lambda|mu smoothing: each iteration does a positive lambda pass then a negative mu pass
// (mu < -lambda), which low-passes the surface while resisting the shrinkage of plain Laplacian.
inline std::vector<math::vec3> smoothMeshTaubin(const std::vector<math::vec3>& positions,
                                               const std::vector<std::uint32_t>& indices,
                                               int iterations = 5, float lambda = 0.5f,
                                               float mu = -0.53f, bool pinBoundary = true) {
    std::vector<math::vec3> pos = positions;
    if (pos.empty()) return pos;
    std::vector<std::vector<std::uint32_t>> nbr;
    std::vector<bool> boundary;
    detail::buildAdjacency(pos.size(), indices, nbr, boundary);
    for (int i = 0; i < iterations; ++i) {
        detail::laplacianPass(pos, nbr, boundary, lambda, pinBoundary);
        detail::laplacianPass(pos, nbr, boundary, mu, pinBoundary);
    }
    return pos;
}

} // namespace maz::render
