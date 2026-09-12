#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

namespace maz::game {

// Flow-field (vector-field) pathfinding — the crowd-movement technique the per-agent A* of Godot's
// NavigationServer (and Maz's own NavGrid, M57) doesn't provide. When MANY agents share ONE goal you
// don't path each of them: you run a single Dijkstra OUTWARD from the goal to get an INTEGRATION FIELD
// (least cost-to-goal for every cell), then bake a FLOW FIELD — each cell stores a unit direction
// pointing down that cost gradient toward the goal. Every agent then navigates for free: it just reads
// the direction under its feet and walks. So a thousand units route around walls to the goal for the
// price of one search, and the paths update in one pass when the goal moves. Pure grid math (8-connected
// Dijkstra, diagonal cost sqrt(2), no corner cutting), deterministic and GPU-free, so it unit-tests
// headlessly.
class FlowField {
public:
    static constexpr float kUnreachable = 1e30f;

    int width() const { return w_; }
    int height() const { return h_; }
    int index(int x, int y) const { return y * w_ + x; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }

    // Build the integration + flow fields for `goal`, over a `w`x`h` grid where blocked[y*w+x] != 0 marks
    // a wall. Cells that cannot reach the goal are left kUnreachable with zero flow.
    void build(int w, int h, const std::vector<std::uint8_t>& blocked, int goalX, int goalY) {
        w_ = w > 0 ? w : 0;
        h_ = h > 0 ? h : 0;
        const std::size_t n = static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_);
        blocked_ = blocked;
        blocked_.resize(n, 0);
        cost_.assign(n, kUnreachable);
        flow_.assign(n, math::vec2(0.0f, 0.0f));
        if (!inBounds(goalX, goalY) || isBlocked(goalX, goalY)) {
            return;
        }

        static constexpr int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static constexpr int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        const float kDiag = 1.41421356f;

        // Dijkstra outward from the goal. Stable ties via insertion sequence.
        struct Node {
            float c;
            std::uint32_t seq;
            int i;
        };
        struct Cmp {
            bool operator()(const Node& a, const Node& b) const {
                return a.c != b.c ? a.c > b.c : a.seq > b.seq;
            }
        };
        std::priority_queue<Node, std::vector<Node>, Cmp> pq;
        std::uint32_t seq = 0;
        const int gi = index(goalX, goalY);
        cost_[static_cast<std::size_t>(gi)] = 0.0f;
        pq.push({0.0f, seq++, gi});

        while (!pq.empty()) {
            const Node cur = pq.top();
            pq.pop();
            if (cur.c > cost_[static_cast<std::size_t>(cur.i)] + 1e-6f) {
                continue; // stale
            }
            const int cx = cur.i % w_;
            const int cy = cur.i / w_;
            for (int k = 0; k < 8; ++k) {
                const int nx = cx + dx[k];
                const int ny = cy + dy[k];
                if (!inBounds(nx, ny) || isBlocked(nx, ny)) {
                    continue;
                }
                const bool diag = k >= 4;
                if (diag && (isBlocked(cx + dx[k], cy) || isBlocked(cx, cy + dy[k]))) {
                    continue; // no corner cutting
                }
                const float nc = cur.c + (diag ? kDiag : 1.0f);
                const std::size_t ni = static_cast<std::size_t>(index(nx, ny));
                if (nc < cost_[ni] - 1e-6f) {
                    cost_[ni] = nc;
                    pq.push({nc, seq++, index(nx, ny)});
                }
            }
        }

        // Bake the flow: each reachable walkable cell points to its lowest-cost eligible neighbour.
        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                const std::size_t i = static_cast<std::size_t>(index(x, y));
                if (isBlocked(x, y) || cost_[i] >= kUnreachable) {
                    continue;
                }
                float best = cost_[i];
                int bx = x, by = y;
                for (int k = 0; k < 8; ++k) {
                    const int nx = x + dx[k];
                    const int ny = y + dy[k];
                    if (!inBounds(nx, ny) || isBlocked(nx, ny)) {
                        continue;
                    }
                    if (k >= 4 && (isBlocked(x + dx[k], y) || isBlocked(x, y + dy[k]))) {
                        continue;
                    }
                    const float nc = cost_[static_cast<std::size_t>(index(nx, ny))];
                    if (nc < best) {
                        best = nc;
                        bx = nx;
                        by = ny;
                    }
                }
                if (bx != x || by != y) {
                    math::vec2 d(static_cast<float>(bx - x), static_cast<float>(by - y));
                    const float l = std::sqrt(d.x * d.x + d.y * d.y);
                    flow_[i] = l > 1e-6f ? d / l : math::vec2(0.0f, 0.0f);
                }
            }
        }
    }

    float costAt(int x, int y) const {
        return inBounds(x, y) ? cost_[static_cast<std::size_t>(index(x, y))] : kUnreachable;
    }
    math::vec2 flowAt(int x, int y) const {
        return inBounds(x, y) ? flow_[static_cast<std::size_t>(index(x, y))] : math::vec2(0.0f, 0.0f);
    }
    bool reachable(int x, int y) const {
        return inBounds(x, y) && cost_[static_cast<std::size_t>(index(x, y))] < kUnreachable;
    }

    // Flow direction at a world position, mapping through `cellSize` and grid `origin` to the containing
    // cell (nearest). Returns zero outside the grid or on unreachable/blocked cells.
    math::vec2 sampleFlow(math::vec2 world, float cellSize, math::vec2 origin = math::vec2(0.0f)) const {
        if (cellSize <= 0.0f) {
            return math::vec2(0.0f, 0.0f);
        }
        const int x = static_cast<int>(std::floor((world.x - origin.x) / cellSize));
        const int y = static_cast<int>(std::floor((world.y - origin.y) / cellSize));
        return flowAt(x, y);
    }

private:
    bool isBlocked(int x, int y) const { return blocked_[static_cast<std::size_t>(index(x, y))] != 0; }

    int w_ = 0;
    int h_ = 0;
    std::vector<std::uint8_t> blocked_;
    std::vector<float> cost_;
    std::vector<math::vec2> flow_;
};

} // namespace maz::game
