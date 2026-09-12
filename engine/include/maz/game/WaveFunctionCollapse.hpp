#pragma once

// maz::game Wave Function Collapse — constraint-based procedural tile generation. Given a palette of
// tile types and a set of adjacency rules ("water may sit next to sand, sand next to grass, but water
// never touches grass"), it fills a grid so that EVERY neighbouring pair obeys the rules, producing
// coherent, non-repeating layouts from a few local constraints. It is the modern procgen technique
// behind hand-authored-looking dungeons, tilesets, and decoration placement — distinct from the engine's
// other generators (CellularCave carves caves, L-systems grow plants, noise makes fields): WFC solves a
// constraint problem. The solver keeps each cell as a superposition of still-possible tiles, repeatedly
// collapses the lowest-entropy cell to a single (weighted-random) tile, and propagates the consequences
// to neighbours by arc-consistency; a contradiction triggers a re-seeded retry. Godot has no WFC, so
// this is a beyond-Godot procgen utility. Deterministic under a fixed seed. Header-only, std-only.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "maz/core/Pcg32.hpp"

namespace maz::game {

// Adjacency rules over up to 64 tile types. Directions: 0 = +x (right), 1 = -x (left), 2 = +y (down),
// 3 = -y (up). allowedMask[dir][a] is the bitset of tiles permitted to sit in direction `dir` from `a`.
struct WfcRules {
    int tileCount = 0;
    std::vector<std::uint64_t> allowedMask[4]; // each sized tileCount

    explicit WfcRules(int tiles) : tileCount(tiles) {
        for (auto& m : allowedMask) m.assign(static_cast<std::size_t>(tiles), 0);
    }

    static int opposite(int dir) { return dir ^ 1; } // 0<->1, 2<->3

    // Permit tile `b` to sit in direction `dir` from tile `a` (and, symmetrically, `a` from `b`).
    void allow(int a, int b, int dir) {
        allowedMask[dir][static_cast<std::size_t>(a)] |= (std::uint64_t{1} << b);
        allowedMask[opposite(dir)][static_cast<std::size_t>(b)] |= (std::uint64_t{1} << a);
    }

    // Convenience: permit `a` and `b` to be neighbours in BOTH axes (symmetric, orientation-free tiles).
    void allowBoth(int a, int b) {
        allow(a, b, 0);
        allow(a, b, 2);
    }
};

struct WfcResult {
    std::vector<int> tiles; // row-major, size width*height; tile id per cell (valid only if success)
    int width = 0;
    int height = 0;
    bool success = false;
    int tileAt(int x, int y) const { return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)]; }
};

namespace detail {

// Union of the neighbour tiles allowed by any tile currently possible in `mask`, in direction `dir`.
inline std::uint64_t allowedNeighbours(const WfcRules& r, std::uint64_t mask, int dir) {
    std::uint64_t out = 0;
    while (mask) {
        const int a = std::countr_zero(mask);
        out |= r.allowedMask[dir][static_cast<std::size_t>(a)];
        mask &= mask - 1;
    }
    return out;
}

} // namespace detail

// Generate a width x height tiling satisfying `rules`. `weights` (optional, size tileCount) biases the
// collapse choice; empty means uniform. Retries up to maxAttempts times on contradiction, each with an
// advanced RNG. Returns success=false if every attempt hit a contradiction.
inline WfcResult wfcGenerate(const WfcRules& rules, int width, int height, std::uint64_t seed,
                            const std::vector<float>& weights = {}, int maxAttempts = 30) {
    WfcResult result;
    result.width = width;
    result.height = height;
    if (rules.tileCount <= 0 || rules.tileCount > 64 || width <= 0 || height <= 0) return result;

    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::uint64_t full = (rules.tileCount == 64) ? ~std::uint64_t{0}
                                                       : ((std::uint64_t{1} << rules.tileCount) - 1);
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        core::Pcg32 rng(seed + static_cast<std::uint64_t>(attempt), 0xC0FFEEu);
        std::vector<std::uint64_t> cell(n, full);
        bool contradiction = false;

        auto propagate = [&](std::vector<std::size_t> stack) {
            while (!stack.empty()) {
                const std::size_t ci = stack.back();
                stack.pop_back();
                const int cx = static_cast<int>(ci % static_cast<std::size_t>(width));
                const int cy = static_cast<int>(ci / static_cast<std::size_t>(width));
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx[d], ny = cy + dy[d];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    const std::size_t ni = static_cast<std::size_t>(ny) * static_cast<std::size_t>(width)
                                         + static_cast<std::size_t>(nx);
                    const std::uint64_t allowed = detail::allowedNeighbours(rules, cell[ci], d);
                    const std::uint64_t before = cell[ni];
                    const std::uint64_t after = before & allowed;
                    if (after != before) {
                        cell[ni] = after;
                        if (after == 0) { contradiction = true; return; }
                        stack.push_back(ni);
                    }
                }
            }
        };

        while (!contradiction) {
            // Find the lowest-entropy undecided cell (fewest possibilities > 1).
            std::size_t best = n;
            int bestCount = 65;
            for (std::size_t i = 0; i < n; ++i) {
                const int c = std::popcount(cell[i]);
                if (c > 1 && c < bestCount) {
                    bestCount = c;
                    best = i;
                }
            }
            if (best == n) break; // everything collapsed -> done

            // Collapse `best` to a single tile, weighted if weights were supplied.
            std::uint64_t m = cell[best];
            int chosen = -1;
            if (weights.empty()) {
                const int k = std::popcount(m);
                int pick = static_cast<int>(rng.nextBounded(static_cast<std::uint32_t>(k)));
                std::uint64_t mm = m;
                while (pick-- >= 0) { chosen = std::countr_zero(mm); mm &= mm - 1; }
            } else {
                float total = 0.0f;
                std::uint64_t mm = m;
                while (mm) { total += weights[static_cast<std::size_t>(std::countr_zero(mm))]; mm &= mm - 1; }
                float r = rng.nextFloat() * total;
                mm = m;
                while (mm) {
                    const int t = std::countr_zero(mm);
                    r -= weights[static_cast<std::size_t>(t)];
                    if (r <= 0.0f) { chosen = t; break; }
                    mm &= mm - 1;
                }
                if (chosen < 0) chosen = std::countr_zero(m);
            }
            cell[best] = std::uint64_t{1} << chosen;
            propagate({best});
        }

        if (!contradiction) {
            result.tiles.assign(n, -1);
            for (std::size_t i = 0; i < n; ++i) {
                if (cell[i] == 0) { contradiction = true; break; }
                result.tiles[i] = std::countr_zero(cell[i]);
            }
            if (!contradiction) {
                result.success = true;
                return result;
            }
        }
    }
    return result;
}

} // namespace maz::game
