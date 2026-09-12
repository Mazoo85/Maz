#pragma once

#include "maz/render/MeshSurfaceSample.hpp" // SurfacePoint, sampleSurfacePoints
#include "maz/render/MeshWeld.hpp"           // detail::cellHash (spatial-hash key)

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render POISSON-DISK PRUNE — thin a dense cloud of surface points down to an EVENLY-SPACED (blue-noise)
// subset: keep a point only if it is at least `minDistance` away from every point already kept. This is the step
// that turns raw surface samples (from `sampleSurfacePoints`, which are random and therefore clumpy — some points
// almost on top of each other, some gaps) into the tidy, no-two-too-close scatter you want when placing grass
// blades, pebbles, trees, bullet-holes/decals, or crowd spawn points across a mesh. Godot calls this "poisson
// disk sampling"; the classic use is "scatter N items on this surface but never let two overlap."
//
// The method is dart-elimination with a spatial hash: walk the input in order and accept each point unless a
// kept point already sits within `minDistance`, using a grid keyed by `minDistance`-sized cells so the "is
// anything too close?" test only ever looks at the 27 neighbouring cells (O(1) expected, O(n) total). Because it
// walks in order, the INPUT ORDER is the priority order — the first point in a cluster wins its spot and the
// rest are dropped. `sampleSurfacePoints` already returns points in random order, so feeding its output straight
// in gives an unbiased blue-noise result; if you built the list some other structured way, shuffle it first.
// Header-only, std-only, deterministic (same input + same radius -> same kept subset).
namespace maz::render {

// Return the subset of `points` no two of which are closer than `minDistance`, keeping earlier points over later
// ones. minDistance <= 0 keeps everything (nothing is "too close"). The kept points retain their position AND
// normal, so they are ready to orient scattered instances.
inline std::vector<SurfacePoint> prunePointsPoisson(const std::vector<SurfacePoint>& points, float minDistance) {
    std::vector<SurfacePoint> kept;
    if (points.empty()) return kept;
    if (minDistance <= 0.0f) { kept = points; return kept; }

    const float cell = minDistance;
    const float minD2 = minDistance * minDistance;

    // Grid cell -> indices into `kept` that fall in that cell. Collisions only add extra distance-checked
    // candidates, never lose one, so correctness does not depend on hash quality.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid;
    grid.reserve(points.size());
    kept.reserve(points.size());

    auto cellOf = [cell](float v) { return static_cast<long>(std::floor(static_cast<double>(v) / cell)); };

    for (const SurfacePoint& p : points) {
        const long cx = cellOf(p.position.x), cy = cellOf(p.position.y), cz = cellOf(p.position.z);
        bool tooClose = false;
        for (long dz = -1; dz <= 1 && !tooClose; ++dz)
            for (long dy = -1; dy <= 1 && !tooClose; ++dy)
                for (long dx = -1; dx <= 1 && !tooClose; ++dx) {
                    const auto it = grid.find(detail::cellHash(cx + dx, cy + dy, cz + dz));
                    if (it == grid.end()) continue;
                    for (std::uint32_t ki : it->second) {
                        const math::vec3 d = kept[ki].position - p.position;
                        if (d.x * d.x + d.y * d.y + d.z * d.z < minD2) { tooClose = true; break; }
                    }
                }
        if (tooClose) continue;
        const std::uint32_t newIndex = static_cast<std::uint32_t>(kept.size());
        kept.push_back(p);
        grid[detail::cellHash(cx, cy, cz)].push_back(newIndex);
    }
    return kept;
}

// Convenience: sample `oversample` random points on `mesh`, then prune to a blue-noise subset with spacing
// `minDistance`. `oversample` should comfortably exceed the number of points that fit at that spacing (the extra
// darts are what the pruning thins away); the returned count is whatever survives.
inline std::vector<SurfacePoint> scatterBlueNoise(const shapes::MeshData& mesh, float minDistance,
                                                  std::size_t oversample, std::uint64_t seed = 1u) {
    return prunePointsPoisson(sampleSurfacePoints(mesh, oversample, seed), minDistance);
}

} // namespace maz::render
