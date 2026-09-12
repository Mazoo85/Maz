#pragma once

#include "maz/math/Math.hpp"       // vec2
#include "maz/math/VectorInt.hpp"  // Vector2i

#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

// maz::game grid ray traversal — the Amanatides & Woo "fast voxel traversal" DDA in 2D. Unlike the
// integer Bresenham line (GridLine), this marches a CONTINUOUS float-coordinate ray through the unit
// grid, visiting every cell the ray actually passes through in the exact order it crosses them. That
// sub-cell precision is what tile raycasters (Wolfenstein-style walls), light/sound propagation,
// projectile sweeps, and precise "which tile does this shot hit first" queries need. The world is a
// unit grid: cell (i, j) covers [i, i+1) x [j, j+1); a point maps to cell (floor x, floor y). Pure
// float+integer math, deterministic, header-only. Godot leaves grid raycasting to the game.
namespace maz::game {

using RayCell = math::Vector2i;

// Every grid cell the ray origin + t*dir (t in [0, maxDistance]) passes through, in crossing order,
// starting with the cell containing `origin`. `dir` need not be normalised — `maxDistance` is measured
// in units of `dir`'s length (pass a normalised dir for world-distance semantics). A zero-length dir
// or non-positive maxDistance yields just the origin cell.
inline std::vector<RayCell> traverseGrid(const math::vec2& origin, const math::vec2& dir,
                                         float maxDistance) {
    std::vector<RayCell> cells;
    int cellX = static_cast<int>(std::floor(origin.x));
    int cellY = static_cast<int>(std::floor(origin.y));
    cells.push_back(RayCell(cellX, cellY));
    const float len2 = dir.x * dir.x + dir.y * dir.y;
    if (len2 <= 0.0f || maxDistance <= 0.0f) {
        return cells;
    }
    const float inf = std::numeric_limits<float>::infinity();

    int stepX = 0;
    float tMaxX = inf;
    float tDeltaX = inf;
    if (dir.x > 0.0f) {
        stepX = 1;
        tMaxX = (static_cast<float>(cellX) + 1.0f - origin.x) / dir.x;
        tDeltaX = 1.0f / dir.x;
    } else if (dir.x < 0.0f) {
        stepX = -1;
        tMaxX = (origin.x - static_cast<float>(cellX)) / (-dir.x);
        tDeltaX = 1.0f / (-dir.x);
    }

    int stepY = 0;
    float tMaxY = inf;
    float tDeltaY = inf;
    if (dir.y > 0.0f) {
        stepY = 1;
        tMaxY = (static_cast<float>(cellY) + 1.0f - origin.y) / dir.y;
        tDeltaY = 1.0f / dir.y;
    } else if (dir.y < 0.0f) {
        stepY = -1;
        tMaxY = (origin.y - static_cast<float>(cellY)) / (-dir.y);
        tDeltaY = 1.0f / (-dir.y);
    }

    while (true) {
        const float nextT = (tMaxX < tMaxY) ? tMaxX : tMaxY;
        if (nextT > maxDistance) {
            break;
        }
        if (tMaxX < tMaxY) {
            cellX += stepX;
            tMaxX += tDeltaX;
        } else {
            cellY += stepY;
            tMaxY += tDeltaY;
        }
        cells.push_back(RayCell(cellX, cellY));
    }
    return cells;
}

// March the ray through the grid and return the first cell for which `blocked(cell)` is true (the tile
// the ray hits), or nullopt if none within `maxDistance`. Includes the origin cell, so a ray starting
// inside a blocked cell returns that cell.
inline std::optional<RayCell> raycastGrid(const math::vec2& origin, const math::vec2& dir,
                                          float maxDistance,
                                          const std::function<bool(const RayCell&)>& blocked) {
    const std::vector<RayCell> cells = traverseGrid(origin, dir, maxDistance);
    for (const RayCell& c : cells) {
        if (blocked(c)) {
            return c;
        }
    }
    return std::nullopt;
}

} // namespace maz::game
