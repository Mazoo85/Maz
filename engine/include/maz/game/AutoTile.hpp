#pragma once

#include "maz/core/Random.hpp"

#include <cstdint>
#include <vector>

namespace maz::game {

// Procedural cave generation + tilemap autotiling — the pieces behind Godot's TileMap terrain
// (autotiling) sets and the classic cellular-automata cave generator. A grid stores 1 = solid (wall),
// 0 = open (floor). CellularCave grows organic caverns from seeded noise; autotileMask4 turns the grid
// into per-cell edge bitmasks so a renderer can pick the right border tile. Pure logic (only core::Random)
// — deterministic under a seed, so it unit-tests headlessly and a given seed always yields the same cave.

// A cellular-automata cave: random fill, then smoothing passes where a cell becomes solid iff it has a
// majority of solid neighbours (the "4-5 rule"). Borders are forced solid so the cave is enclosed.
class CellularCave {
public:
    // Generate a w×h grid (row-major, 1=wall/0=floor). `wallProb` is the initial random fill; `steps`
    // smoothing passes; `seed` makes it reproducible.
    static std::vector<uint8_t> generate(int w, int h, uint64_t seed, float wallProb = 0.45f,
                                         int steps = 5) {
        std::vector<uint8_t> grid(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 1);
        if (w <= 0 || h <= 0) {
            return grid;
        }
        core::Random rng(seed);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const bool border = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
                grid[idx(w, x, y)] = (border || rng.nextFloat() < wallProb) ? 1u : 0u;
            }
        }
        std::vector<uint8_t> next = grid;
        for (int s = 0; s < steps; ++s) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const bool border = (x == 0 || y == 0 || x == w - 1 || y == h - 1);
                    next[idx(w, x, y)] =
                        border ? 1u : (countWalls(grid, w, h, x, y) >= 5 ? 1u : 0u);
                }
            }
            grid.swap(next);
        }
        return grid;
    }

    // Count solid cells in the 8-neighbourhood; out-of-bounds counts as solid (keeps edges enclosed).
    static int countWalls(const std::vector<uint8_t>& g, int w, int h, int x, int y) {
        int n = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int cx = x + dx, cy = y + dy;
                if (cx < 0 || cy < 0 || cx >= w || cy >= h || g[idx(w, cx, cy)] != 0) {
                    ++n;
                }
            }
        }
        return n;
    }

    static std::size_t idx(int w, int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
    }
};

// 4-bit edge mask of a solid cell: which of its 4-connected neighbours (N,E,S,W) are ALSO solid.
// bit0=N, bit1=E, bit2=S, bit3=W. Out-of-bounds counts as solid, so a wall on the map edge reads as
// "connected" on that side and doesn't grow a spurious border. This is the value autotiling keys on:
// a tileset lays out 16 border variants indexed by this mask.
inline uint8_t autotileMask4(const std::vector<uint8_t>& solid, int w, int h, int x, int y) {
    auto s = [&](int cx, int cy) {
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) {
            return true; // out of bounds = solid
        }
        return solid[CellularCave::idx(w, cx, cy)] != 0;
    };
    uint8_t m = 0;
    if (s(x, y - 1)) m |= 0x1; // N
    if (s(x + 1, y)) m |= 0x2; // E
    if (s(x, y + 1)) m |= 0x4; // S
    if (s(x - 1, y)) m |= 0x8; // W
    return m;
}

// Tile index for a 16-tile "blob" autotile atlas laid out in mask order (atlas cell i == mask i).
inline int autotileIndex4(uint8_t mask4) { return static_cast<int>(mask4 & 0x0f); }

} // namespace maz::game
