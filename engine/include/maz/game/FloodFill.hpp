#pragma once

#include "maz/math/VectorInt.hpp"

#include <cstddef>
#include <functional>
#include <vector>

// maz::game flood fill & connected regions — the grid "paint bucket" and its region-labelling
// companion. Flood fill collects every cell reachable from a seed through passable cells (BFS,
// 4- or 8-connected): the paint-bucket tool, "which tiles can the player actually reach from here",
// spill/water fill, and enclosed-area detection. Connected regions partitions ALL passable cells of a
// grid into separate components — counting rooms/islands, finding the biggest cavern of a procedural
// map, or pruning unreachable pockets. Pure integer grid math on a caller-supplied passability
// predicate, deterministic (scan/BFS order), header-only. Godot leaves this to the game, so it is a
// genuinely-useful utility with an exact, testable output.
namespace maz::game {

using FillCell = math::Vector2i;

namespace detail {

// 4-neighbour offsets, then the 4 diagonals (used when `diagonal` is true).
inline const int kFillDX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
inline const int kFillDY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

// BFS from `start` over `passable` cells, marking `visited` (row-major, size width*height) and
// appending every reached cell to `out`. Assumes start is in-bounds, passable and unvisited.
inline void floodBfs(int width, int height, FillCell start,
                     const std::function<bool(const FillCell&)>& passable, bool diagonal,
                     std::vector<char>& visited, std::vector<FillCell>& out) {
    const int neighbourCount = diagonal ? 8 : 4;
    std::vector<FillCell> frontier;
    frontier.push_back(start);
    visited[static_cast<std::size_t>(start.y) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(start.x)] = 1;
    while (!frontier.empty()) {
        const FillCell c = frontier.back();
        frontier.pop_back();
        out.push_back(c);
        for (int n = 0; n < neighbourCount; ++n) {
            const int nx = c.x + kFillDX[n];
            const int ny = c.y + kFillDY[n];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                continue;
            }
            const std::size_t idx =
                static_cast<std::size_t>(ny) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(nx);
            if (visited[idx]) {
                continue;
            }
            const FillCell nc(nx, ny);
            if (!passable(nc)) {
                continue;
            }
            visited[idx] = 1;
            frontier.push_back(nc);
        }
    }
}

} // namespace detail

// Every in-bounds cell reachable from `start` through `passable` cells, 4-connected (or 8-connected
// when `diagonal` is true). Returns an empty list when `start` is out of bounds or not passable.
inline std::vector<FillCell> floodFill(int width, int height, FillCell start,
                                       const std::function<bool(const FillCell&)>& passable,
                                       bool diagonal = false) {
    std::vector<FillCell> out;
    if (width <= 0 || height <= 0 || start.x < 0 || start.y < 0 || start.x >= width ||
        start.y >= height || !passable(start)) {
        return out;
    }
    std::vector<char> visited(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    detail::floodBfs(width, height, start, passable, diagonal, visited, out);
    return out;
}

// Partition all passable cells of a `width` x `height` grid into connected components, 4-connected
// (or 8-connected when `diagonal` is true). Each region is the list of its cells; regions are ordered
// by the row-major scan position of their first-discovered cell, so the output is deterministic.
inline std::vector<std::vector<FillCell>> connectedRegions(
    int width, int height, const std::function<bool(const FillCell&)>& passable,
    bool diagonal = false) {
    std::vector<std::vector<FillCell>> regions;
    if (width <= 0 || height <= 0) {
        return regions;
    }
    std::vector<char> visited(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t idx =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x);
            if (visited[idx]) {
                continue;
            }
            const FillCell c(x, y);
            if (!passable(c)) {
                continue;
            }
            std::vector<FillCell> region;
            detail::floodBfs(width, height, c, passable, diagonal, visited, region);
            regions.push_back(std::move(region));
        }
    }
    return regions;
}

} // namespace maz::game
