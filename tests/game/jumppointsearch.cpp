// tests/game/jumppointsearch.cpp — verifies Jump Point Search (game JumpPointSearch.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a straight open corridor gives a straight path; a wall forces the expected detour;
//   * JPS returns EXACTLY the same optimal path COST as the engine's AStarGrid2D (DiagonalMode::Always),
//     checked on thousands of random obstacle grids — the two agree on reachability and on cost every time;
//   * every JPS path is valid: endpoints correct, in bounds, never on a solid cell, each step a single
//     8-neighbour move;
//   * on a wide-open grid JPS expands far fewer cells than the grid holds (the whole point of JPS);
//   * the sparse jump-point path expands back into that same full cell path.
#include "maz/game/JumpPointSearch.hpp"
#include "maz/game/AStarGrid2D.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::AStarGrid2D;
using maz::game::DiagonalMode;
using maz::game::GridHeuristic;
using maz::game::JumpPointSearch;
using maz::math::Vector2i;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

// Cost of a full cell path under the 1 / sqrt(2) model.
static double pathCost(const std::vector<Vector2i>& p) {
    const double s2 = std::sqrt(2.0);
    double c = 0.0;
    for (std::size_t i = 1; i < p.size(); ++i) {
        const int dx = std::abs(p[i].x - p[i - 1].x);
        const int dy = std::abs(p[i].y - p[i - 1].y);
        c += (dx != 0 && dy != 0) ? s2 : 1.0;
    }
    return c;
}

int main() {
    // --- 1. Open corridor + a simple wall detour. ---
    {
        JumpPointSearch jps;
        jps.setSize(7, 1);
        const auto p = jps.findPath({0, 0}, {6, 0});
        CHECK(p.size() == 7 && p.front().x == 0 && p.back().x == 6, "straight open corridor");

        JumpPointSearch jw;
        jw.setSize(5, 5);
        for (int y = 0; y < 4; ++y) jw.setSolid(2, y, true); // vertical wall, gap at bottom row
        const auto pw = jw.findPath({0, 0}, {4, 0});
        CHECK(!pw.empty(), "path exists around the wall");
        CHECK(pw.front().x == 0 && pw.front().y == 0 && pw.back().x == 4 && pw.back().y == 0,
              "detour endpoints correct");
    }

    // --- 2. No path when the goal is walled off. ---
    {
        JumpPointSearch jps;
        jps.setSize(5, 5);
        for (int y = 0; y < 5; ++y) jps.setSolid(2, y, true); // full vertical wall splits the grid
        CHECK(jps.findPath({0, 0}, {4, 4}).empty(), "no path through a full wall");
    }

    // --- 3. Randomised cost/reachability agreement with AStarGrid2D(Always). ---
    {
        Lcg rng{0xA57C0DEu};
        bool ok = true;
        int reachable = 0;
        for (int trial = 0; trial < 3000 && ok; ++trial) {
            const int w = 4 + static_cast<int>(rng.next() % 22u); // 4..25
            const int h = 4 + static_cast<int>(rng.next() % 22u);
            JumpPointSearch jps;
            AStarGrid2D as;
            jps.setSize(w, h);
            as.setSize(w, h);
            as.setDiagonalMode(DiagonalMode::Always);
            as.setHeuristic(GridHeuristic::Octile);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (rng.next() % 100u < 28u) { // ~28% obstacles
                        jps.setSolid(x, y, true);
                        as.setSolid(x, y, true);
                    }
                }
            }
            Vector2i from{static_cast<int>(rng.next() % static_cast<std::uint32_t>(w)),
                          static_cast<int>(rng.next() % static_cast<std::uint32_t>(h))};
            Vector2i to{static_cast<int>(rng.next() % static_cast<std::uint32_t>(w)),
                        static_cast<int>(rng.next() % static_cast<std::uint32_t>(h))};
            if (jps.isSolid(from.x, from.y) || jps.isSolid(to.x, to.y)) continue;

            const auto pj = jps.findPath(from, to);
            const auto pa = as.getPointPath(from, to);

            if (pa.empty() != pj.empty()) { ok = false; break; } // must agree on reachability
            if (pa.empty()) continue;
            ++reachable;

            // Cost parity (JPS is optimal for this model, so equal cost to A*).
            if (std::abs(pathCost(pj) - pathCost(pa)) > 1e-3) { ok = false; break; }

            // JPS path validity: endpoints, in-bounds, not solid, single-step moves.
            if (!(pj.front().x == from.x && pj.front().y == from.y &&
                  pj.back().x == to.x && pj.back().y == to.y)) { ok = false; break; }
            for (std::size_t i = 0; i < pj.size(); ++i) {
                if (!jps.inBounds(pj[i]) || jps.isSolid(pj[i].x, pj[i].y)) { ok = false; break; }
                if (i > 0) {
                    const int dx = std::abs(pj[i].x - pj[i - 1].x);
                    const int dy = std::abs(pj[i].y - pj[i - 1].y);
                    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) { ok = false; break; }
                }
            }
        }
        CHECK(ok, "JPS matches AStarGrid2D on cost + reachability across random grids");
        CHECK(reachable > 500, "the random trials actually exercised reachable paths");
    }

    // --- 4. JPS efficiency: on an open grid it expands only a handful of cells. ---
    {
        JumpPointSearch jps;
        jps.setSize(50, 50); // 2500 cells
        const auto p = jps.findPath({0, 0}, {49, 49});
        CHECK(!p.empty(), "open-grid diagonal path found");
        CHECK(jps.lastExpanded() < 100, "JPS expands far fewer than the 2500 grid cells");
    }

    // --- 5. Sparse jump points expand back into the full contiguous path. ---
    {
        JumpPointSearch jps;
        jps.setSize(12, 12);
        for (int y = 2; y < 10; ++y) jps.setSolid(6, y, true); // an obstacle to create real turns
        const auto jpts = jps.findJumpPoints({0, 0}, {11, 11});
        const auto full = jps.findPath({0, 0}, {11, 11});
        CHECK(!jpts.empty() && !full.empty(), "both sparse and full paths exist");
        CHECK(jpts.size() <= full.size(), "jump points are a sparse subset of the full path");
        // Expanding the sparse points must reproduce the full path.
        std::vector<Vector2i> expanded;
        if (!jpts.empty()) {
            expanded.push_back(jpts.front());
            for (std::size_t i = 1; i < jpts.size(); ++i) {
                const Vector2i a = jpts[i - 1], b = jpts[i];
                const int dx = (b.x > a.x) - (b.x < a.x);
                const int dy = (b.y > a.y) - (b.y < a.y);
                int x = a.x, y = a.y;
                while (x != b.x || y != b.y) { x += dx; y += dy; expanded.push_back({x, y}); }
            }
        }
        bool same = expanded.size() == full.size();
        for (std::size_t i = 0; same && i < full.size(); ++i)
            if (expanded[i].x != full[i].x || expanded[i].y != full[i].y) same = false;
        CHECK(same, "expanding the jump points reproduces the full path");
    }

    if (g_fail == 0) {
        std::printf("jumppointsearch: OK — corridor, walls, A* cost parity, efficiency, sparse expansion.\n");
        return 0;
    }
    std::printf("jumppointsearch: %d failure(s).\n", g_fail);
    return 1;
}
