#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <queue>       // std::priority_queue
#include <algorithm>   // std::reverse
#include <limits>      // std::numeric_limits
#include <tuple>       // std::tie

#include "maz/core/Assert.hpp"

// 2D grid A* pathfinder (Godot AStarGrid2D analog). Integer costs (10 orthogonal,
// 14 diagonal) so results are exact/deterministic — no floating-point drift, no
// platform variance. Uses an admissible+consistent Manhattan heuristic for
// 4-connected search and an octile heuristic for 8-connected search, both scaled
// to the 10/14 cost model. Diagonal moves FORBID corner-cutting: a diagonal step
// is only legal when both orthogonally-adjacent cells are walkable, so a path can
// never slip between two blocked corners. The open set uses a deterministic
// tie-break (lowest f, then lowest h, then lowest cell index) so an unbroken run
// on the same inputs always returns the same path. findPath returns the
// cost-optimal start..goal-inclusive path, or an empty path when the goal is
// unreachable or start|goal is blocked; start==goal yields a single-cell path.
// NOT thread-safe. navmesh, weighted/variable terrain cost, JPS (jump-point
// search), and multi-goal queries are future refinements (not built here).

namespace maz::ai {

struct GridCoord {
    std::int32_t x = 0;
    std::int32_t y = 0;
    constexpr bool operator==(const GridCoord& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const GridCoord& o) const { return !(*this == o); }
};

// A rectangular occupancy grid: every cell is either walkable or blocked. Cells
// start walkable; setBlocked toggles them. Out-of-bounds cells are never walkable.
class GridMap {
  public:
    GridMap(std::int32_t width, std::int32_t height)
        : m_w(width), m_h(height),
          m_blocked(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), false) {
        MAZ_ASSERT(width > 0 && height > 0, "GridMap: width and height must be positive");
    }

    std::int32_t width() const { return m_w; }
    std::int32_t height() const { return m_h; }

    bool inBounds(GridCoord c) const { return c.x >= 0 && c.x < m_w && c.y >= 0 && c.y < m_h; }

    // Out-of-bounds -> NOT walkable (and never touches m_blocked out of range).
    bool walkable(GridCoord c) const { return inBounds(c) && !m_blocked[index(c)]; }

    void setBlocked(GridCoord c, bool blocked) {
        MAZ_ASSERT(inBounds(c), "GridMap::setBlocked: coord out of bounds");
        m_blocked[index(c)] = blocked;
    }

  private:
    // Row-major flat index. Only valid when inBounds(c); callers guarantee that.
    std::size_t index(GridCoord c) const {
        return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(c.x);
    }

    std::int32_t m_w, m_h;
    std::vector<bool> m_blocked;
};

// Integer step costs — exact, so all comparisons are deterministic.
constexpr std::int32_t kOrthoCost = 10;
constexpr std::int32_t kDiagCost = 14;

// Cost-optimal A* over the grid. See file header for the full contract.
inline std::vector<GridCoord> findPath(const GridMap& map, GridCoord start, GridCoord goal,
                                       bool allowDiagonal = false) {
    if (!map.walkable(start) || !map.walkable(goal)) { return {}; }
    if (start == goal) { return { start }; }

    const std::int32_t w = map.width();
    const std::int32_t h = map.height();
    const std::size_t cellCount = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

    // Flat index <-> coord helpers (only used with in-bounds coords/indices).
    const auto toIdx = [w](GridCoord c) -> std::int32_t { return c.y * w + c.x; };
    const auto toCoord = [w](std::int32_t idx) -> GridCoord {
        return GridCoord{ idx % w, idx / w };
    };

    // Local abs on int32 — avoids std::abs overload ambiguity / -Wconversion noise.
    const auto heuristic = [allowDiagonal](GridCoord c, GridCoord g) -> std::int32_t {
        const std::int32_t dx = c.x >= g.x ? c.x - g.x : g.x - c.x;
        const std::int32_t dy = c.y >= g.y ? c.y - g.y : g.y - c.y;
        if (allowDiagonal) {
            const std::int32_t lo = dx < dy ? dx : dy;
            const std::int32_t hi = dx < dy ? dy : dx;
            return kDiagCost * lo + kOrthoCost * (hi - lo);  // octile
        }
        return kOrthoCost * (dx + dy);  // Manhattan
    };

    std::vector<std::int32_t> g(cellCount, std::numeric_limits<std::int32_t>::max());
    std::vector<std::int32_t> parent(cellCount, -1);  // -1 == no parent
    std::vector<char> closed(cellCount, 0);

    struct Node { std::int32_t f, h, idx; };
    // Min-heap on (f, then h, then idx): priority_queue is a max-heap, so the
    // comparator returns true when 'a' should be popped AFTER 'b'.
    struct NodeCmp {
        bool operator()(const Node& a, const Node& b) const {
            return std::tie(a.f, a.h, a.idx) > std::tie(b.f, b.h, b.idx);
        }
    };
    std::priority_queue<Node, std::vector<Node>, NodeCmp> open;

    const std::int32_t startIdx = toIdx(start);
    const std::int32_t goalIdx = toIdx(goal);
    g[static_cast<std::size_t>(startIdx)] = 0;
    {
        const std::int32_t sh = heuristic(start, goal);
        open.push(Node{ sh, sh, startIdx });
    }

    // 8 candidate steps: first 4 orthogonal, last 4 diagonal.
    constexpr std::int32_t dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    constexpr std::int32_t dys[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    const int stepCount = allowDiagonal ? 8 : 4;

    bool reached = false;
    while (!open.empty()) {
        const Node n = open.top();
        open.pop();
        const std::size_t ni = static_cast<std::size_t>(n.idx);
        if (closed[ni]) { continue; }  // lazy deletion: stale entry
        closed[ni] = 1;
        if (n.idx == goalIdx) { reached = true; break; }

        const GridCoord c = toCoord(n.idx);
        for (int s = 0; s < stepCount; ++s) {
            const GridCoord nb{ c.x + dxs[s], c.y + dys[s] };
            if (!map.walkable(nb)) { continue; }
            const bool diagonal = dxs[s] != 0 && dys[s] != 0;
            if (diagonal) {
                // Forbid corner-cutting: both orthogonal cells sharing this
                // diagonal must be walkable, else the move slips between corners.
                if (!map.walkable(GridCoord{ c.x + dxs[s], c.y }) ||
                    !map.walkable(GridCoord{ c.x, c.y + dys[s] })) {
                    continue;
                }
            }
            const std::int32_t stepCost = diagonal ? kDiagCost : kOrthoCost;
            const std::int32_t nbIdx = toIdx(nb);
            const std::size_t nbi = static_cast<std::size_t>(nbIdx);
            const std::int32_t tentative = g[ni] + stepCost;
            if (tentative < g[nbi]) {
                g[nbi] = tentative;
                parent[nbi] = n.idx;
                const std::int32_t nh = heuristic(nb, goal);
                open.push(Node{ tentative + nh, nh, nbIdx });
            }
        }
    }

    if (!reached) { return {}; }

    // Reconstruct start..goal by walking parents backward, then reverse.
    std::vector<GridCoord> path;
    for (std::int32_t idx = goalIdx; idx != -1; idx = parent[static_cast<std::size_t>(idx)]) {
        path.push_back(toCoord(idx));
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// Sum of step costs over consecutive cells (kDiagCost for a diagonal pair,
// kOrthoCost otherwise). Empty or single-cell path -> 0. Helper for tests/callers.
inline std::int32_t pathCost(const std::vector<GridCoord>& path, bool allowDiagonal) {
    (void)allowDiagonal;  // step type is inferred from the cells; kept for call symmetry
    std::int32_t cost = 0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const std::int32_t dx = path[i].x - path[i - 1].x;
        const std::int32_t dy = path[i].y - path[i - 1].y;
        cost += (dx != 0 && dy != 0) ? kDiagCost : kOrthoCost;
    }
    return cost;
}

} // namespace maz::ai
