#pragma once

#include "maz/math/Math.hpp"       // vec3
#include "maz/math/VectorInt.hpp"  // Vector3i

#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

// maz::game 3D voxel ray traversal — the Amanatides & Woo "A Fast Voxel Traversal Algorithm" (1987): walk a
// ray through a 3D grid of unit cells and visit EVERY voxel it passes through, in order, with no gaps and no
// duplicates. This is the 3D companion to GridRaycast (which is 2D) and the workhorse behind block-world
// interaction: which block is the player looking at / mining / placing against, 3D line-of-sight and light
// propagation through a voxel volume, and ray-marching a sparse voxel scene. A naive "step along the ray in
// small increments" either skips thin voxels (steps too big) or visits the same voxel many times (steps too
// small) and drifts off the true line; this advances exactly to the next cell boundary each iteration, so it
// is both exact and O(number of voxels crossed). Cells are unit-sized; the integer cell of a point p is
// floor(p) per axis. Header-only, std-only, deterministic. Godot ships no voxel traversal.
namespace maz::game {

inline int voxelFloor(float v) { return static_cast<int>(std::floor(static_cast<double>(v))); }

// Ordered list of voxels a ray from `origin` in direction `dir` passes through, starting at the voxel
// containing `origin` and continuing while the travelled distance is <= maxDistance (and cell count <
// maxCells). `dir` need not be normalized (distance is measured in world units regardless).
inline std::vector<math::Vector3i> traverseVoxels(const math::vec3& origin, const math::vec3& dir,
                                                  float maxDistance, std::size_t maxCells = 4096) {
    std::vector<math::Vector3i> out;
    const double dx = dir.x, dy = dir.y, dz = dir.z;
    const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-20 || maxDistance <= 0.0f || maxCells == 0) {
        return out;
    }
    // Normalized direction so tMax/tDelta are in world-distance units.
    const double nx = dx / len, ny = dy / len, nz = dz / len;

    int cx = voxelFloor(origin.x), cy = voxelFloor(origin.y), cz = voxelFloor(origin.z);
    const int stepX = (nx > 0.0) - (nx < 0.0);
    const int stepY = (ny > 0.0) - (ny < 0.0);
    const int stepZ = (nz > 0.0) - (nz < 0.0);

    const double kInf = 1e300;
    // Distance to the first voxel boundary on each axis, and the per-voxel distance step.
    auto firstBoundary = [](double o, int cell, int step, double n) -> double {
        if (step == 0) {
            return 1e300;
        }
        const double next = (step > 0) ? (static_cast<double>(cell) + 1.0) : static_cast<double>(cell);
        return (next - o) / n; // n has the sign of step, so this is positive
    };
    double tMaxX = firstBoundary(origin.x, cx, stepX, nx);
    double tMaxY = firstBoundary(origin.y, cy, stepY, ny);
    double tMaxZ = firstBoundary(origin.z, cz, stepZ, nz);
    const double tDeltaX = (stepX != 0) ? std::fabs(1.0 / nx) : kInf;
    const double tDeltaY = (stepY != 0) ? std::fabs(1.0 / ny) : kInf;
    const double tDeltaZ = (stepZ != 0) ? std::fabs(1.0 / nz) : kInf;

    double t = 0.0;
    while (t <= static_cast<double>(maxDistance) && out.size() < maxCells) {
        out.push_back(math::Vector3i{cx, cy, cz});
        // Advance to the nearest axis boundary.
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            cx += stepX;
            t = tMaxX;
            tMaxX += tDeltaX;
        } else if (tMaxY <= tMaxZ) {
            cy += stepY;
            t = tMaxY;
            tMaxY += tDeltaY;
        } else {
            cz += stepZ;
            t = tMaxZ;
            tMaxZ += tDeltaZ;
        }
    }
    return out;
}

// Cast a ray until `isSolid(cell)` is true; returns the first solid voxel hit, or nullopt within range.
inline std::optional<math::Vector3i> voxelRaycast(const math::vec3& origin, const math::vec3& dir,
                                                  float maxDistance,
                                                  const std::function<bool(const math::Vector3i&)>& isSolid,
                                                  std::size_t maxCells = 4096) {
    const std::vector<math::Vector3i> cells = traverseVoxels(origin, dir, maxDistance, maxCells);
    for (const math::Vector3i& c : cells) {
        if (isSolid(c)) {
            return c;
        }
    }
    return std::nullopt;
}

} // namespace maz::game
