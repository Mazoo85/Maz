#pragma once

#include "maz/core/Pcg32.hpp"
#include "maz/math/VectorInt.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

// maz::game maze generation — a "perfect" maze via the recursive-backtracker (depth-first) algorithm.
// A perfect maze has exactly one path between any two cells: fully connected, no loops, no isolated
// pockets. The classic look for puzzle levels, hedge mazes, and pipe/wire layouts, and a different
// flavour again from BSP dungeons (rooms) and cellular caves (organic blobs). The result is rendered
// as a tile grid of (2*width+1) x (2*height+1) cells: odd coordinates are cell centres, even ones are
// the walls between them; a wall tile is carved to floor exactly when the DFS connects the two cells
// it separates. Deterministic for a given seed (core::Pcg32), pure integer grid math, header-only.
// Godot leaves procedural generation to the game.
namespace maz::game {

// The generated maze as a wall/floor tile grid. `width`/`height` are the maze size in CELLS; the tile
// grid is `tileWidth` x `tileHeight` = (2*width+1) x (2*height+1), row-major, 0 = wall / 1 = floor.
struct Maze {
    int width = 0;      // cells
    int height = 0;     // cells
    int tileWidth = 0;  // 2*width + 1
    int tileHeight = 0; // 2*height + 1
    std::vector<std::uint8_t> tiles;

    bool floorAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= tileWidth || y >= tileHeight) {
            return false;
        }
        return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(tileWidth) +
                     static_cast<std::size_t>(x)] != 0;
    }
};

// Generate a perfect maze of `width` x `height` cells, deterministic for `seed`. Returns an empty maze
// when either dimension is non-positive.
inline Maze generateMaze(int width, int height, std::uint64_t seed) {
    Maze m;
    if (width <= 0 || height <= 0) {
        return m;
    }
    m.width = width;
    m.height = height;
    m.tileWidth = 2 * width + 1;
    m.tileHeight = 2 * height + 1;
    m.tiles.assign(static_cast<std::size_t>(m.tileWidth) * static_cast<std::size_t>(m.tileHeight), 0);

    auto carve = [&](int tx, int ty) {
        m.tiles[static_cast<std::size_t>(ty) * static_cast<std::size_t>(m.tileWidth) +
                static_cast<std::size_t>(tx)] = 1;
    };

    std::vector<char> visited(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    auto visitedAt = [&](int cx, int cy) -> char& {
        return visited[static_cast<std::size_t>(cy) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(cx)];
    };

    core::Pcg32 rng(seed, 0x94d049bb133111ebULL);
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    std::vector<math::Vector2i> stack;
    stack.push_back(math::Vector2i(0, 0));
    visitedAt(0, 0) = 1;
    carve(1, 1); // cell (0,0) centre

    while (!stack.empty()) {
        const math::Vector2i cur = stack.back();
        int candidates[4];
        int count = 0;
        for (int d = 0; d < 4; ++d) {
            const int nx = cur.x + dx[d];
            const int ny = cur.y + dy[d];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                continue;
            }
            if (visitedAt(nx, ny)) {
                continue;
            }
            candidates[count++] = d;
        }
        if (count == 0) {
            stack.pop_back();
            continue;
        }
        const int pick = candidates[rng.nextBounded(static_cast<std::uint32_t>(count))];
        const int nx = cur.x + dx[pick];
        const int ny = cur.y + dy[pick];
        // Carve the neighbour cell centre and the wall tile between the two cells.
        carve(2 * nx + 1, 2 * ny + 1);
        carve(2 * cur.x + 1 + dx[pick], 2 * cur.y + 1 + dy[pick]);
        visitedAt(nx, ny) = 1;
        stack.push_back(math::Vector2i(nx, ny));
    }
    return m;
}

} // namespace maz::game
