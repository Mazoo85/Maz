// tests/game/thetastar.cpp — verifies Theta* any-angle pathfinding (game ThetaStar.hpp).
// Ground truths, deterministic (fixed grids, no <random>, no clock):
//   * OPEN GRID: the path across an empty grid collapses to a single straight line of the exact Euclidean
//     length (the any-angle property grid A* cannot achieve);
//   * OPTIMALITY BOUND: the Theta* path is never longer than the grid-constrained (8-connected) shortest
//     path found by an independent Dijkstra — verified with an obstacle in the way;
//   * VALID SEGMENTS: no path segment passes through a blocked cell (checked by an INDEPENDENT dense
//     sampler, not the module's own line-of-sight);
//   * a fully walled-off goal is reported unreachable;
//   * determinism.
#include "maz/game/ThetaStar.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <utility>
#include <vector>

using maz::game::ThetaPath;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float pathLength(const std::vector<std::pair<int, int>>& wp) {
    float len = 0.0f;
    for (std::size_t i = 1; i < wp.size(); ++i) {
        const float dx = static_cast<float>(wp[i].first - wp[i - 1].first);
        const float dy = static_cast<float>(wp[i].second - wp[i - 1].second);
        len += std::sqrt(dx * dx + dy * dy);
    }
    return len;
}

// Independent segment-clear check: densely sample the segment and confirm it never passes through the BODY
// of a blocked cell (within 0.35 of a wall cell's centre). This tolerates the corner-grazing that standard
// Theta* permits at convex obstacle tips while catching any segment that genuinely tunnels through a wall.
static bool segmentClear(const std::vector<std::uint8_t>& grid, int w, int h, int x0, int y0, int x1, int y1) {
    const int steps = 400;
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const float fx = static_cast<float>(x0) + t * static_cast<float>(x1 - x0);
        const float fy = static_cast<float>(y0) + t * static_cast<float>(y1 - y0);
        const int cx = static_cast<int>(std::lround(fx)), cy = static_cast<int>(std::lround(fy));
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) return false;
        if (grid[static_cast<std::size_t>(cy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(cx)]) {
            const float ddx = fx - static_cast<float>(cx), ddy = fy - static_cast<float>(cy);
            if (std::sqrt(ddx * ddx + ddy * ddy) < 0.35f) return false; // tunnels through the wall body
        }
    }
    return true;
}

// Independent 8-connected Dijkstra (no corner cut) giving the grid-constrained optimum.
static float gridDijkstra(const std::vector<std::uint8_t>& grid, int w, int h, int sx, int sy, int gx, int gy) {
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    std::vector<float> d(n, 1e30f);
    auto id = [&](int x, int y) { return static_cast<std::size_t>(y * w + x); };
    using QN = std::pair<float, int>;
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> pq;
    d[id(sx, sy)] = 0.0f;
    pq.push({0.0f, static_cast<int>(id(sx, sy))});
    const int dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dys[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!pq.empty()) {
        const auto [dc, cur] = pq.top();
        pq.pop();
        if (dc > d[static_cast<std::size_t>(cur)]) continue;
        const int cx = cur % w, cy = cur / w;
        for (int k = 0; k < 8; ++k) {
            const int nx = cx + dxs[k], ny = cy + dys[k];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || grid[id(nx, ny)]) continue;
            if (dxs[k] && dys[k] && (grid[id(cx + dxs[k], cy)] || grid[id(cx, cy + dys[k])])) continue;
            const float step = (dxs[k] && dys[k]) ? 1.41421356f : 1.0f;
            if (dc + step < d[id(nx, ny)]) {
                d[id(nx, ny)] = dc + step;
                pq.push({dc + step, static_cast<int>(id(nx, ny))});
            }
        }
    }
    return d[id(gx, gy)];
}

int main() {
    // --- 1. Open grid: straight line of exact Euclidean length. ---
    {
        const int w = 20, h = 20;
        std::vector<std::uint8_t> grid(static_cast<std::size_t>(w * h), 0);
        const ThetaPath p = maz::game::thetaStar(grid, w, h, 0, 0, 19, 13);
        CHECK(p.found, "a path is found across an open grid");
        const float euclid = std::sqrt(19.0f * 19.0f + 13.0f * 13.0f);
        CHECK(std::fabs(pathLength(p.waypoints) - euclid) < 1e-3f,
              "the open-grid path has the exact straight-line length (any-angle)");
        CHECK(p.waypoints.size() == 2, "the open-grid path is a single straight segment");
    }

    // --- 2 & 3. Obstacle: optimality bound + valid segments. ---
    {
        const int w = 25, h = 25;
        std::vector<std::uint8_t> grid(static_cast<std::size_t>(w * h), 0);
        // A vertical wall at x=12 from y=0..18, leaving a gap near the bottom.
        for (int y = 0; y <= 18; ++y) grid[static_cast<std::size_t>(y * w + 12)] = 1;
        const ThetaPath p = maz::game::thetaStar(grid, w, h, 2, 5, 22, 5);
        CHECK(p.found, "a path is found around the wall");
        const float thetaLen = pathLength(p.waypoints);
        const float gridOpt = gridDijkstra(grid, w, h, 2, 5, 22, 5);
        CHECK(thetaLen <= gridOpt + 1e-3f, "Theta* is never longer than the grid-constrained optimum");
        CHECK(thetaLen < gridOpt - 0.5f, "Theta* is strictly shorter than the staircase grid path here");
        bool clear = true, connected = p.waypoints.front() == std::make_pair(2, 5) &&
                                       p.waypoints.back() == std::make_pair(22, 5);
        for (std::size_t i = 1; i < p.waypoints.size(); ++i)
            if (!segmentClear(grid, w, h, p.waypoints[i - 1].first, p.waypoints[i - 1].second,
                              p.waypoints[i].first, p.waypoints[i].second))
                clear = false;
        CHECK(clear, "no path segment crosses a blocked cell (independent check)");
        CHECK(connected, "the path starts at the start and ends at the goal");
    }

    // --- 4. Unreachable goal. ---
    {
        const int w = 10, h = 10;
        std::vector<std::uint8_t> grid(static_cast<std::size_t>(w * h), 0);
        // Wall off the goal cell (9,9) completely.
        grid[static_cast<std::size_t>(8 * w + 9)] = 1;
        grid[static_cast<std::size_t>(9 * w + 8)] = 1;
        grid[static_cast<std::size_t>(8 * w + 8)] = 1;
        const ThetaPath p = maz::game::thetaStar(grid, w, h, 0, 0, 9, 9);
        CHECK(!p.found, "a walled-off goal is reported unreachable");
    }

    // --- 5. Determinism. ---
    {
        const int w = 15, h = 15;
        std::vector<std::uint8_t> grid(static_cast<std::size_t>(w * h), 0);
        for (int y = 3; y <= 11; ++y) grid[static_cast<std::size_t>(y * w + 7)] = 1;
        const ThetaPath a = maz::game::thetaStar(grid, w, h, 1, 1, 13, 13);
        const ThetaPath b = maz::game::thetaStar(grid, w, h, 1, 1, 13, 13);
        bool same = a.found == b.found && a.waypoints.size() == b.waypoints.size();
        for (std::size_t i = 0; same && i < a.waypoints.size(); ++i)
            if (a.waypoints[i] != b.waypoints[i]) same = false;
        CHECK(same, "identical inputs produce identical paths");
    }

    if (g_fail == 0) {
        std::printf("thetastar: OK — straight open path, optimality bound, valid segments, unreachable, determinism.\n");
        return 0;
    }
    std::printf("thetastar: %d failure(s).\n", g_fail);
    return 1;
}
