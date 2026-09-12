#pragma once

#include "maz/math/VectorInt.hpp" // Vector2i

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <utility>
#include <vector>

// maz::game AStarGrid2D — Godot's AStarGrid2D: A* pathfinding specialized for a dense rectangular grid
// of walkable/solid cells. Unlike the general weighted-graph AStar2D (arbitrary points + links) or the
// simple orthogonal-only NavGrid, this adds the two features that make grid pathfinding feel right:
// selectable DIAGONAL movement rules (never / only through open corners / around single obstacles /
// always) and selectable HEURISTICS (Euclidean / Manhattan / Octile / Chebyshev). Header-only, pure,
// deterministic — unit-tested cell by cell.
namespace maz::game {

// How diagonal moves are permitted relative to the two orthogonal cells they "cut" past.
enum class DiagonalMode {
    Never,               // 4-connected only
    OnlyIfNoObstacles,   // diagonal allowed only when BOTH flanking orthogonal cells are walkable
    AtLeastOneWalkable,  // diagonal allowed when AT LEAST ONE flanking orthogonal cell is walkable
    Always               // diagonal always allowed (can cut corners)
};

// Distance estimate used for the heuristic (and the step cost).
enum class GridHeuristic { Euclidean, Manhattan, Octile, Chebyshev };

class AStarGrid2D {
public:
    void setSize(int width, int height) {
        m_w = width < 0 ? 0 : width;
        m_h = height < 0 ? 0 : height;
        m_solid.assign(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), 0);
    }
    int width() const { return m_w; }
    int height() const { return m_h; }

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }
    bool inBounds(const math::Vector2i& p) const { return inBounds(p.x, p.y); }

    void setSolid(int x, int y, bool solid) {
        if (inBounds(x, y)) {
            m_solid[idx(x, y)] = solid ? 1u : 0u;
        }
    }
    bool isSolid(int x, int y) const { return inBounds(x, y) && m_solid[idx(x, y)] != 0u; }
    void clearSolids() { std::fill(m_solid.begin(), m_solid.end(), static_cast<std::uint8_t>(0)); }

    void setDiagonalMode(DiagonalMode m) { m_diag = m; }
    DiagonalMode diagonalMode() const { return m_diag; }
    void setHeuristic(GridHeuristic h) { m_heuristic = h; }
    GridHeuristic heuristic() const { return m_heuristic; }

    // The cell path from `from` to `to` inclusive, or empty if either endpoint is out of bounds/solid
    // or no route exists (Godot's AStarGrid2D.get_id_path / get_point_path).
    std::vector<math::Vector2i> getPointPath(const math::Vector2i& from,
                                             const math::Vector2i& to) const {
        std::vector<math::Vector2i> out;
        if (!inBounds(from) || !inBounds(to) || isSolid(from.x, from.y) || isSolid(to.x, to.y)) {
            return out;
        }
        const int start = idxRaw(from.x, from.y);
        const int goal = idxRaw(to.x, to.y);
        if (start == goal) {
            out.push_back(from);
            return out;
        }

        const std::size_t n = m_solid.size();
        std::vector<float> g(n, kInf);
        std::vector<int> came(n, -1);
        std::vector<std::uint8_t> closed(n, 0);

        using Node = std::pair<float, int>; // (f, cell index)
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        g[static_cast<std::size_t>(start)] = 0.0f;
        open.push({h(from, to), start});

        while (!open.empty()) {
            const int cur = open.top().second;
            open.pop();
            if (closed[static_cast<std::size_t>(cur)]) {
                continue;
            }
            closed[static_cast<std::size_t>(cur)] = 1;
            if (cur == goal) {
                break;
            }
            const int cx = cur % m_w;
            const int cy = cur / m_w;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = cx + dx;
                    const int ny = cy + dy;
                    if (!inBounds(nx, ny) || isSolid(nx, ny)) {
                        continue;
                    }
                    const bool diagonal = (dx != 0 && dy != 0);
                    if (diagonal && !diagonalAllowed(cx, cy, dx, dy)) {
                        continue;
                    }
                    const int ni = idxRaw(nx, ny);
                    if (closed[static_cast<std::size_t>(ni)]) {
                        continue;
                    }
                    const float step = diagonal ? kSqrt2 : 1.0f;
                    const float tentative = g[static_cast<std::size_t>(cur)] + step;
                    if (tentative < g[static_cast<std::size_t>(ni)]) {
                        g[static_cast<std::size_t>(ni)] = tentative;
                        came[static_cast<std::size_t>(ni)] = cur;
                        const math::Vector2i np{nx, ny};
                        open.push({tentative + h(np, to), ni});
                    }
                }
            }
        }

        if (came[static_cast<std::size_t>(goal)] == -1 && goal != start) {
            return out; // unreachable
        }
        // Reconstruct start->goal.
        std::vector<math::Vector2i> rev;
        for (int c = goal; c != -1; c = came[static_cast<std::size_t>(c)]) {
            rev.push_back(math::Vector2i{c % m_w, c / m_w});
            if (c == start) {
                break;
            }
        }
        out.assign(rev.rbegin(), rev.rend());
        return out;
    }

private:
    static constexpr float kInf = 1e30f;
    static constexpr float kSqrt2 = 1.41421356f;

    int idxRaw(int x, int y) const { return y * m_w + x; }
    std::size_t idx(int x, int y) const { return static_cast<std::size_t>(idxRaw(x, y)); }

    bool diagonalAllowed(int cx, int cy, int dx, int dy) const {
        switch (m_diag) {
        case DiagonalMode::Never:
            return false;
        case DiagonalMode::Always:
            return true;
        case DiagonalMode::OnlyIfNoObstacles:
            return !isSolid(cx + dx, cy) && !isSolid(cx, cy + dy);
        case DiagonalMode::AtLeastOneWalkable:
            return !isSolid(cx + dx, cy) || !isSolid(cx, cy + dy);
        }
        return false;
    }

    float h(const math::Vector2i& a, const math::Vector2i& b) const {
        const float dx = static_cast<float>(std::abs(a.x - b.x));
        const float dy = static_cast<float>(std::abs(a.y - b.y));
        switch (m_heuristic) {
        case GridHeuristic::Euclidean:
            return std::sqrt(dx * dx + dy * dy);
        case GridHeuristic::Manhattan:
            return dx + dy;
        case GridHeuristic::Chebyshev:
            return dx > dy ? dx : dy;
        case GridHeuristic::Octile: {
            const float mn = dx < dy ? dx : dy;
            const float mx = dx > dy ? dx : dy;
            return mx + (kSqrt2 - 1.0f) * mn;
        }
        }
        return 0.0f;
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<std::uint8_t> m_solid;
    DiagonalMode m_diag = DiagonalMode::Always;
    GridHeuristic m_heuristic = GridHeuristic::Euclidean;
};

} // namespace maz::game
