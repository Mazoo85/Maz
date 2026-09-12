#pragma once

#include "maz/math/VectorInt.hpp" // Vector2i

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

// maz::game JumpPointSearch — Jump Point Search (Harabor & Grastien, 2011): an optimisation of A* for
// UNIFORM-COST 8-connected grids that returns the exact same optimal path as plain grid A* while
// expanding dramatically fewer nodes. Ordinary A* on an open grid wastes almost all its work exploring
// the countless equivalent zig-zag routes between two points (path "symmetries"). JPS eliminates that by
// "jumping" in a straight line along each direction, skipping over every cell that couldn't possibly be a
// turning point, and only ever placing a handful of genuine decision cells (jump points) on the open list.
// On a wide-open map JPS is routinely 10-30x faster than the same A* — the difference between a smooth RTS
// with hundreds of pathing units and a stuttering one. Maz already ships AStarGrid2D (Dijkstra-grade grid
// A*); this is its high-performance sibling for the common case of a uniform grid where every step costs 1
// (orthogonal) or sqrt(2) (diagonal). Godot has no JPS at all.
//
// Movement model: 8-connected, straight cost 1, diagonal cost sqrt(2), corner-cutting ALLOWED (a diagonal
// step is legal whenever its target cell is free — matching AStarGrid2D's DiagonalMode::Always). That
// equivalence is exactly what makes the result checkable: JPS here yields the same path cost as
// AStarGrid2D(Always) on any grid. Header-only, pure, deterministic.
namespace maz::game {

class JumpPointSearch {
public:
    void setSize(int width, int height) {
        m_w = width < 0 ? 0 : width;
        m_h = height < 0 ? 0 : height;
        m_solid.assign(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), 0u);
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

    // How many cells the last findPath()/findJumpPoints() pulled off the open list (popped and expanded).
    // A JPS-vs-A* efficiency probe: on open grids this is a tiny fraction of what plain A* expands.
    std::size_t lastExpanded() const { return m_expanded; }

    // The turning points of the optimal route: start, each jump point, goal (Godot-style sparse path).
    // Empty if either endpoint is out of bounds/solid or no route exists.
    std::vector<math::Vector2i> findJumpPoints(const math::Vector2i& from,
                                               const math::Vector2i& to) const {
        std::vector<math::Vector2i> out;
        m_expanded = 0;
        if (!inBounds(from) || !inBounds(to) || isSolid(from.x, from.y) || isSolid(to.x, to.y)) {
            return out;
        }
        const int start = raw(from.x, from.y);
        const int goal = raw(to.x, to.y);
        if (start == goal) {
            out.push_back(from);
            return out;
        }

        const std::size_t n = m_solid.size();
        std::vector<float> g(n, kInf);
        std::vector<int> came(n, -1);
        std::vector<std::uint8_t> closed(n, 0u);

        using Node = std::pair<float, int>; // (f, cell)
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        g[static_cast<std::size_t>(start)] = 0.0f;
        open.push({octile(from.x, from.y, to.x, to.y), start});

        while (!open.empty()) {
            const int cur = open.top().second;
            open.pop();
            if (closed[static_cast<std::size_t>(cur)]) {
                continue;
            }
            closed[static_cast<std::size_t>(cur)] = 1u;
            ++m_expanded;
            if (cur == goal) {
                break;
            }
            const int cx = cur % m_w;
            const int cy = cur / m_w;

            // Arrival direction (0,0 at the start) drives the pruned successor set.
            int pdx = 0, pdy = 0;
            const int parent = came[static_cast<std::size_t>(cur)];
            if (parent != -1) {
                const int px = parent % m_w;
                const int py = parent / m_w;
                pdx = sign(cx - px);
                pdy = sign(cy - py);
            }

            for (const auto& d : successorDirs(cx, cy, pdx, pdy)) {
                int jx = 0, jy = 0;
                if (!jump(cx, cy, d.first, d.second, to.x, to.y, jx, jy)) {
                    continue;
                }
                const int ji = raw(jx, jy);
                if (closed[static_cast<std::size_t>(ji)]) {
                    continue;
                }
                const int adx = std::abs(jx - cx);
                const int ady = std::abs(jy - cy);
                const int steps = adx > ady ? adx : ady;         // pure straight/diagonal line
                const bool diag = (d.first != 0 && d.second != 0);
                const float segCost = static_cast<float>(steps) * (diag ? kSqrt2 : 1.0f);
                const float tentative = g[static_cast<std::size_t>(cur)] + segCost;
                if (tentative < g[static_cast<std::size_t>(ji)]) {
                    g[static_cast<std::size_t>(ji)] = tentative;
                    came[static_cast<std::size_t>(ji)] = cur;
                    open.push({tentative + octile(jx, jy, to.x, to.y), ji});
                }
            }
        }

        if (came[static_cast<std::size_t>(goal)] == -1) {
            return out; // unreachable
        }
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

    // The full inclusive cell path (every intermediate cell filled in between jump points), directly
    // comparable to AStarGrid2D::getPointPath. Empty if no route.
    std::vector<math::Vector2i> findPath(const math::Vector2i& from, const math::Vector2i& to) const {
        const std::vector<math::Vector2i> jps = findJumpPoints(from, to);
        std::vector<math::Vector2i> out;
        if (jps.empty()) {
            return out;
        }
        out.push_back(jps.front());
        for (std::size_t i = 1; i < jps.size(); ++i) {
            const math::Vector2i& a = jps[i - 1];
            const math::Vector2i& b = jps[i];
            const int dx = sign(b.x - a.x);
            const int dy = sign(b.y - a.y);
            int x = a.x, y = a.y;
            while (x != b.x || y != b.y) {
                x += dx;
                y += dy;
                out.push_back(math::Vector2i{x, y});
            }
        }
        return out;
    }

private:
    static constexpr float kInf = 1e30f;
    static constexpr float kSqrt2 = 1.41421356237f;

    int raw(int x, int y) const { return y * m_w + x; }
    std::size_t idx(int x, int y) const { return static_cast<std::size_t>(raw(x, y)); }
    bool passable(int x, int y) const { return inBounds(x, y) && m_solid[idx(x, y)] == 0u; }
    static int sign(int v) { return (v > 0) - (v < 0); }

    // Octile distance — admissible & consistent for the 1 / sqrt(2) cost model.
    static float octile(int ax, int ay, int bx, int by) {
        const int dx = std::abs(ax - bx);
        const int dy = std::abs(ay - by);
        const int mn = dx < dy ? dx : dy;
        const int mx = dx > dy ? dx : dy;
        return static_cast<float>(mx - mn) + kSqrt2 * static_cast<float>(mn);
    }

    // Scan in one direction until a jump point (goal, or a cell with a forced neighbour, or — while moving
    // diagonally — a cell from which a straight scan finds one) is hit. Corner-cutting-allowed rules.
    bool jump(int x0, int y0, int dx, int dy, int gx, int gy, int& jx, int& jy) const {
        int x = x0, y = y0;
        for (;;) {
            x += dx;
            y += dy;
            if (!passable(x, y)) {
                return false;
            }
            if (x == gx && y == gy) {
                jx = x;
                jy = y;
                return true;
            }
            if (dx != 0 && dy != 0) { // diagonal
                if ((!passable(x - dx, y) && passable(x - dx, y + dy)) ||
                    (!passable(x, y - dy) && passable(x + dx, y - dy))) {
                    jx = x;
                    jy = y;
                    return true;
                }
                int tx = 0, ty = 0;
                if (jump(x, y, dx, 0, gx, gy, tx, ty) || jump(x, y, 0, dy, gx, gy, tx, ty)) {
                    jx = x;
                    jy = y;
                    return true;
                }
            } else if (dx != 0) { // horizontal
                if ((!passable(x, y + 1) && passable(x + dx, y + 1)) ||
                    (!passable(x, y - 1) && passable(x + dx, y - 1))) {
                    jx = x;
                    jy = y;
                    return true;
                }
            } else { // vertical
                if ((!passable(x + 1, y) && passable(x + 1, y + dy)) ||
                    (!passable(x - 1, y) && passable(x - 1, y + dy))) {
                    jx = x;
                    jy = y;
                    return true;
                }
            }
        }
    }

    // Pruned successor DIRECTIONS given the arrival direction (natural neighbours + forced neighbours).
    std::vector<std::pair<int, int>> successorDirs(int x, int y, int pdx, int pdy) const {
        std::vector<std::pair<int, int>> dirs;
        if (pdx == 0 && pdy == 0) { // start: all eight
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx != 0 || dy != 0) {
                        dirs.emplace_back(dx, dy);
                    }
                }
            }
            return dirs;
        }
        if (pdx != 0 && pdy != 0) { // diagonal arrival
            dirs.emplace_back(pdx, 0);
            dirs.emplace_back(0, pdy);
            dirs.emplace_back(pdx, pdy);
            if (!passable(x - pdx, y)) {
                dirs.emplace_back(-pdx, pdy); // forced
            }
            if (!passable(x, y - pdy)) {
                dirs.emplace_back(pdx, -pdy); // forced
            }
        } else if (pdx != 0) { // horizontal arrival
            dirs.emplace_back(pdx, 0);
            if (!passable(x, y + 1)) {
                dirs.emplace_back(pdx, 1); // forced
            }
            if (!passable(x, y - 1)) {
                dirs.emplace_back(pdx, -1); // forced
            }
        } else { // vertical arrival
            dirs.emplace_back(0, pdy);
            if (!passable(x + 1, y)) {
                dirs.emplace_back(1, pdy); // forced
            }
            if (!passable(x - 1, y)) {
                dirs.emplace_back(-1, pdy); // forced
            }
        }
        return dirs;
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<std::uint8_t> m_solid;
    mutable std::size_t m_expanded = 0;
};

} // namespace maz::game
