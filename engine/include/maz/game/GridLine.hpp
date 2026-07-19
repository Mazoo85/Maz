#pragma once

#include "maz/math/VectorInt.hpp"

#include <cstdlib>
#include <functional>
#include <vector>

// maz::game grid-line utilities — the integer-grid line rasteriser (Bresenham) and the tile
// line-of-sight test built on it. These are the staples of tile/grid games: draw a straight line of
// tiles (lasers, roads, trajectory previews, tile-brush strokes), and answer "can A see B?" across a
// tilemap where some cells block vision (roguelike FOV checks, guard sight cones, cover tests).
// Bresenham steps one cell at a time choosing the axis with the larger delta, so the returned path is
// 8-connected and symmetric-ish; line-of-sight walks that same path and reports whether any cell
// strictly between the endpoints blocks it. Pure integer math, deterministic, header-only — Godot has
// no built-in grid line/LOS helper (TileMap leaves this to the game), so this is a genuinely-useful
// utility with an exact, testable output.
namespace maz::game {

// A grid cell coordinate for line/LOS work. (Distinct from GridMap's 3D GridCell — these are the flat
// 2D tile coordinates a line rasteriser walks; reusing math::Vector2i keeps it interoperable.)
using LineCell = math::Vector2i;

// All grid cells on the Bresenham line from `a` to `b`, inclusive of both endpoints, ordered from
// `a` to `b`. A single point when a == b. 8-connected (diagonal steps allowed).
inline std::vector<LineCell> bresenhamLine(const LineCell& a, const LineCell& b) {
    std::vector<LineCell> cells;
    int x0 = a.x;
    int y0 = a.y;
    const int x1 = b.x;
    const int y1 = b.y;
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy; // error accumulator (dy is negative)
    while (true) {
        cells.push_back(LineCell(x0, y0));
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) { // step in x
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) { // step in y
            err += dx;
            y0 += sy;
        }
    }
    return cells;
}

// True when nothing blocks the straight tile-line from `a` to `b`: walks the Bresenham path and
// returns false if `blocked(cell)` is true for any cell STRICTLY between the endpoints. The endpoints
// themselves are never treated as blockers, so a viewer standing on (or looking at) a wall tile still
// "sees" along the line up to it — the common roguelike convention. a == b is always visible.
inline bool lineOfSight(const LineCell& a, const LineCell& b,
                        const std::function<bool(const LineCell&)>& blocked) {
    const std::vector<LineCell> path = bresenhamLine(a, b);
    if (path.size() <= 2) {
        return true; // adjacent or identical: nothing in between to block
    }
    for (std::size_t i = 1; i + 1 < path.size(); ++i) {
        if (blocked(path[i])) {
            return false;
        }
    }
    return true;
}

} // namespace maz::game
