#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

// maz::game — Wang tiling: lay out tiles so their edges always match, producing large NON-REPEATING
// textures, terrain, dungeons, or road/river networks from a small tile set. Each Wang tile has a colour on
// each of its four edges; two tiles may sit next to each other only if their touching edges share a colour.
// Because the constraint is purely local, you can fill an arbitrarily large grid one tile at a time and the
// result tiles seamlessly yet never falls into an obvious repeating pattern — the classic trick behind
// infinite ground textures, auto-generated mazes, and pipe/track puzzles. This does stochastic scanline
// placement: for each cell it picks (deterministically from a seed) among the tiles whose west edge matches
// the left neighbour's east edge and whose north edge matches the upper neighbour's south edge. With a
// COMPLETE tile set (at least one tile for every west/north colour pair) placement never gets stuck. Godot
// ships no Wang tiler. Header-only, std-only, deterministic.
namespace maz::game {

struct WangTile {
    int north = 0;
    int east = 0;
    int south = 0;
    int west = 0;
};

namespace detail {
inline std::uint64_t wangHash(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
} // namespace detail

// Fill a width x height grid with tile indices such that adjacent tiles share edge colours. Scanline order;
// each cell chooses among the tiles matching its already-placed left and upper neighbours. Returns the grid
// (row-major, size width*height) of indices into `tiles`, or an empty vector if a cell has no valid tile
// (an incomplete tile set) or the inputs are degenerate.
inline std::vector<int> wangTiling(const std::vector<WangTile>& tiles, int width, int height,
                                   std::uint64_t seed) {
    std::vector<int> grid;
    if (tiles.empty() || width <= 0 || height <= 0) {
        return grid;
    }
    grid.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), -1);
    std::vector<int> candidates;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                    static_cast<std::size_t>(x);
            const bool haveWest = x > 0;
            const bool haveNorth = y > 0;
            const int westColor = haveWest ? tiles[static_cast<std::size_t>(grid[idx - 1])].east : 0;
            const int northColor =
                haveNorth
                    ? tiles[static_cast<std::size_t>(grid[idx - static_cast<std::size_t>(width)])].south
                    : 0;
            candidates.clear();
            for (std::size_t t = 0; t < tiles.size(); ++t) {
                if (haveWest && tiles[t].west != westColor) {
                    continue;
                }
                if (haveNorth && tiles[t].north != northColor) {
                    continue;
                }
                candidates.push_back(static_cast<int>(t));
            }
            if (candidates.empty()) {
                return std::vector<int>{}; // stuck: incomplete tile set
            }
            const std::uint64_t h = detail::wangHash(seed ^ detail::wangHash(idx * 2654435761ull + 1u));
            grid[idx] = candidates[static_cast<std::size_t>(h % candidates.size())];
        }
    }
    return grid;
}

} // namespace maz::game
