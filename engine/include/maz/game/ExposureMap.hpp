#pragma once

#include "maz/game/GridLine.hpp" // game::lineOfSight, LineCell
#include "maz/math/VectorInt.hpp" // math::Vector2i

#include <cstddef>
#include <functional>
#include <vector>

// maz::game cover / exposure map — for every open tile, HOW MANY threats can see it. Given a set of threat
// positions (enemy eyes, turrets, guards) and the walls, this shoots a tile line of sight from each threat to
// each cell and counts the ones that reach: the result is a tactical field the AI reads to pick COVER (a cell
// no threat can see), to grade how exposed a position is, or to weight a "safest route" search away from
// sniped ground. A wall casts a SHADOW of zero-exposure cells behind it — exactly the cover a player ducks
// into. Reads its blockers through the same `blocked(cell)` predicate the rest of the grid AI uses and reuses
// the engine's Bresenham `lineOfSight`.
//
// Distinct from the engine's neighbours: FieldOfView computes ONE viewer's visible set (shadowcasting),
// Visibility2D builds a polygon from segment occluders, InfluenceMap is a smooth diffusion with no
// line-of-sight, and DijkstraMap is travel DISTANCE — none answers "how many of these threats have a clear
// shot at this tile?" over the whole grid. Deterministic, header-only. Godot leaves tactical maps to the game.
namespace maz::game {

using ExposureCell = math::Vector2i;

// Per-cell threat-visibility count. `count[y*width + x]` is how many threats have line of sight to that cell.
struct ExposureMap {
    int width = 0;
    int height = 0;
    std::vector<int> count;

    int at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return count[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)];
    }
    // A cell is "covered" when no threat can see it (and it is in bounds).
    bool isCovered(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height && at(x, y) == 0; }
};

// Build the exposure map: for each cell not itself a wall, count how many `threats` have an unobstructed tile
// line of sight to it. Wall cells are left at 0 (you don't take cover inside a wall). Threats off the grid or
// on a wall are skipped.
inline ExposureMap buildExposureMap(int width, int height, const std::vector<ExposureCell>& threats,
                                    const std::function<bool(const ExposureCell&)>& blocked) {
    ExposureMap m;
    m.width = width < 0 ? 0 : width;
    m.height = height < 0 ? 0 : height;
    m.count.assign(static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height), 0);
    if (m.width == 0 || m.height == 0) {
        return m;
    }
    auto idx = [&](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m.width) +
               static_cast<std::size_t>(x);
    };
    for (const ExposureCell& t : threats) {
        if (t.x < 0 || t.y < 0 || t.x >= m.width || t.y >= m.height || blocked(t)) {
            continue; // threat off-grid or embedded in a wall sees nothing useful
        }
        for (int y = 0; y < m.height; ++y) {
            for (int x = 0; x < m.width; ++x) {
                const ExposureCell c(x, y);
                if (blocked(c)) {
                    continue; // walls stay at 0
                }
                if (lineOfSight(t, c, blocked)) {
                    ++m.count[idx(x, y)];
                }
            }
        }
    }
    return m;
}

} // namespace maz::game
