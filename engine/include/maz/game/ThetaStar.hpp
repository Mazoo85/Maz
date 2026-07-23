#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

// maz::game Theta* any-angle pathfinding — a grid path planner that produces SHORT, STRAIGHT routes instead
// of the staircase zig-zag that ordinary grid A* (and jump-point search) is stuck with. Classic grid search
// can only step between cell centres along the 8 compass directions, so a diagonal crossing of an open room
// comes out as a jagged approximation that is both longer and visibly unnatural. Theta* adds a
// line-of-sight test: when relaxing a node it checks whether the node's *grandparent* can see the new cell
// directly, and if so it links straight to it — letting path segments cut across the grid at any angle. The
// result hugs walls tightly and, in open space, collapses to a single straight line of the true Euclidean
// length. This is exactly the "shorter, natural-looking" path Godot's grid navigation can't produce. Uses
// the standard Nash line-of-sight. Header-only, std-only, deterministic. Grid steps are 8-connected with no
// corner cutting; as in standard Theta*, an any-angle shortcut may graze a convex obstacle's outer corner
// but never passes through a wall's body.
namespace maz::game {

struct ThetaPath {
    std::vector<std::pair<int, int>> waypoints; // (x,y) cells from start to goal
    bool found = false;
};

// Integer grid line-of-sight (Nash): true if the segment between cell centres (x0,y0)->(x1,y1) crosses no
// blocked cell. `blocked[y*w + x]` is non-zero for walls; out-of-bounds counts as blocked.
inline bool thetaLineOfSight(const std::vector<std::uint8_t>& blocked, int w, int h, int x0, int y0, int x1,
                             int y1) {
    auto blk = [&](int cx, int cy) {
        return cx < 0 || cy < 0 || cx >= w || cy >= h ||
               blocked[static_cast<std::size_t>(cy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(cx)];
    };
    int dx = x1 - x0, dy = y1 - y0;
    int sx, sy;
    if (dy < 0) { dy = -dy; sy = -1; } else { sy = 1; }
    if (dx < 0) { dx = -dx; sx = -1; } else { sx = 1; }
    const int ox = (sx - 1) / 2, oy = (sy - 1) / 2;
    int f = 0;
    if (dx >= dy) {
        while (x0 != x1) {
            f += dy;
            if (f >= dx) {
                if (blk(x0 + ox, y0 + oy)) return false;
                y0 += sy;
                f -= dx;
            }
            if (f != 0 && blk(x0 + ox, y0 + oy)) return false;
            if (dy == 0 && blk(x0 + ox, y0) && blk(x0 + ox, y0 - 1)) return false;
            x0 += sx;
        }
    } else {
        while (y0 != y1) {
            f += dx;
            if (f >= dy) {
                if (blk(x0 + ox, y0 + oy)) return false;
                x0 += sx;
                f -= dy;
            }
            if (f != 0 && blk(x0 + ox, y0 + oy)) return false;
            if (dx == 0 && blk(x0, y0 + oy) && blk(x0 - 1, y0 + oy)) return false;
            y0 += sy;
        }
    }
    return true;
}

// Plan an any-angle path from (sx,sy) to (gx,gy). Returns found=false if either endpoint is blocked or no
// route exists.
inline ThetaPath thetaStar(const std::vector<std::uint8_t>& blocked, int w, int h, int sx, int sy, int gx,
                           int gy) {
    ThetaPath result;
    if (w <= 0 || h <= 0 || sx < 0 || sy < 0 || gx < 0 || gy < 0 || sx >= w || sy >= h || gx >= w || gy >= h) {
        return result;
    }
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    auto idx = [&](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x); };
    auto isBlk = [&](int x, int y) { return blocked[idx(x, y)] != 0; };
    if (isBlk(sx, sy) || isBlk(gx, gy)) {
        return result;
    }
    auto dist = [](int ax, int ay, int bx, int by) {
        const float ddx = static_cast<float>(ax - bx), ddy = static_cast<float>(ay - by);
        return std::sqrt(ddx * ddx + ddy * ddy);
    };

    std::vector<float> g(n, 1e30f);
    std::vector<int> parent(n, -1);
    std::vector<std::uint8_t> closed(n, 0);
    using QN = std::pair<float, int>; // (f, cellIndex)
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> open;

    const std::size_t start = idx(sx, sy), goal = idx(gx, gy);
    g[start] = 0.0f;
    parent[start] = static_cast<int>(start);
    open.push({dist(sx, sy, gx, gy), static_cast<int>(start)});

    const int dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dys[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    bool reached = false;
    while (!open.empty()) {
        const int cur = open.top().second;
        open.pop();
        if (closed[static_cast<std::size_t>(cur)]) {
            continue;
        }
        closed[static_cast<std::size_t>(cur)] = 1;
        if (static_cast<std::size_t>(cur) == goal) {
            reached = true;
            break;
        }
        const int cx = cur % w, cy = cur / w;
        const int par = parent[static_cast<std::size_t>(cur)];
        const int px = par % w, py = par / w;
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + dxs[d], ny = cy + dys[d];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || isBlk(nx, ny)) {
                continue;
            }
            // No corner cutting on diagonal steps.
            if (dxs[d] != 0 && dys[d] != 0 && (isBlk(cx + dxs[d], cy) || isBlk(cx, cy + dys[d]))) {
                continue;
            }
            const std::size_t ni = idx(nx, ny);
            if (closed[ni]) {
                continue;
            }
            // Path 2: link to the grandparent if it can see the neighbour (any-angle shortcut).
            if (thetaLineOfSight(blocked, w, h, px, py, nx, ny)) {
                const float ng = g[static_cast<std::size_t>(par)] + dist(px, py, nx, ny);
                if (ng < g[ni]) {
                    g[ni] = ng;
                    parent[ni] = par;
                    open.push({ng + dist(nx, ny, gx, gy), static_cast<int>(ni)});
                }
            } else {
                // Path 1: standard grid relaxation.
                const float ng = g[static_cast<std::size_t>(cur)] + dist(cx, cy, nx, ny);
                if (ng < g[ni]) {
                    g[ni] = ng;
                    parent[ni] = cur;
                    open.push({ng + dist(nx, ny, gx, gy), static_cast<int>(ni)});
                }
            }
        }
    }
    if (!reached) {
        return result;
    }
    // Reconstruct.
    std::vector<std::pair<int, int>> rev;
    std::size_t node = goal;
    while (true) {
        rev.push_back({static_cast<int>(node % static_cast<std::size_t>(w)),
                       static_cast<int>(node / static_cast<std::size_t>(w))});
        if (node == start) {
            break;
        }
        node = static_cast<std::size_t>(parent[node]);
    }
    result.waypoints.assign(rev.rbegin(), rev.rend());
    result.found = true;
    return result;
}

} // namespace maz::game
