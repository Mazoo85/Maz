#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

namespace maz::game {

// A uniform 2D navigation grid over the X/Z ground plane with A* pathfinding. Cells are either
// walkable or blocked; findPath returns a least-cost route between two cells using 8-directional
// movement (orthogonal cost 1, diagonal cost sqrt(2)) with an octile-distance heuristic and
// corner-cutting disallowed (a diagonal step is blocked if either orthogonally-adjacent cell it
// squeezes past is solid). World<->cell mapping matches the engine's ground-plane convention: X and
// Z map to grid columns/rows, Y is ignored. Header-only and dependency-free so it unit-tests without
// a GPU.
class NavGrid {
public:
    struct Cell {
        int x = 0;
        int z = 0;
        bool operator==(const Cell& o) const { return x == o.x && z == o.z; }
    };

    NavGrid() = default;
    NavGrid(int width, int height, float cellSize = 1.0f, math::vec3 origin = math::vec3(0.0f)) {
        resize(width, height, cellSize, origin);
    }

    // (Re)allocate the grid. All cells start walkable.
    void resize(int width, int height, float cellSize = 1.0f, math::vec3 origin = math::vec3(0.0f)) {
        m_w = width > 0 ? width : 0;
        m_h = height > 0 ? height : 0;
        m_cell = cellSize > 0.0f ? cellSize : 1.0f;
        m_origin = origin;
        m_blocked.assign(static_cast<size_t>(m_w) * static_cast<size_t>(m_h), 0);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    float cellSize() const { return m_cell; }

    bool inBounds(int x, int z) const { return x >= 0 && z >= 0 && x < m_w && z < m_h; }
    bool inBounds(const Cell& c) const { return inBounds(c.x, c.z); }

    void setBlocked(int x, int z, bool blocked) {
        if (inBounds(x, z)) {
            m_blocked[index(x, z)] = blocked ? 1 : 0;
        }
    }
    bool blocked(int x, int z) const { return !inBounds(x, z) || m_blocked[index(x, z)] != 0; }
    bool walkable(int x, int z) const { return inBounds(x, z) && m_blocked[index(x, z)] == 0; }

    // Block every cell whose center lies inside the world-space X/Z box [minX,maxX]x[minZ,maxZ].
    // Convenience for stamping obstacles (walls, buildings) from their footprint.
    void blockWorldBox(float minX, float minZ, float maxX, float maxZ) {
        for (int z = 0; z < m_h; ++z) {
            for (int x = 0; x < m_w; ++x) {
                const math::vec3 c = cellToWorld({x, z});
                if (c.x >= minX && c.x <= maxX && c.z >= minZ && c.z <= maxZ) {
                    m_blocked[index(x, z)] = 1;
                }
            }
        }
    }

    // World position -> the cell containing it (may be out of bounds; caller can check inBounds).
    Cell worldToCell(const math::vec3& p) const {
        return Cell{static_cast<int>(std::floor((p.x - m_origin.x) / m_cell)),
                    static_cast<int>(std::floor((p.z - m_origin.z) / m_cell))};
    }
    // Cell -> the world position of its center (Y = origin.y).
    math::vec3 cellToWorld(const Cell& c) const {
        return math::vec3(m_origin.x + (static_cast<float>(c.x) + 0.5f) * m_cell, m_origin.y,
                          m_origin.z + (static_cast<float>(c.z) + 0.5f) * m_cell);
    }

    // A* from `start` to `goal`. On success fills `outPath` with cells from start to goal (inclusive)
    // and returns true. Returns false (and clears outPath) if either endpoint is out of bounds or
    // blocked, or no route exists. Deterministic: equal-cost ties break by insertion order.
    bool findPath(const Cell& start, const Cell& goal, std::vector<Cell>& outPath) const {
        outPath.clear();
        if (!walkable(start.x, start.z) || !walkable(goal.x, goal.z)) {
            return false;
        }
        if (start == goal) {
            outPath.push_back(start);
            return true;
        }

        const size_t n = static_cast<size_t>(m_w) * static_cast<size_t>(m_h);
        constexpr float kInf = 1e30f;
        std::vector<float> g(n, kInf);
        std::vector<int32_t> came(n, -1);
        std::vector<uint8_t> closed(n, 0);

        struct Node {
            float f;
            uint32_t seq; // insertion order for stable tie-breaking
            int idx;
        };
        struct Cmp {
            bool operator()(const Node& a, const Node& b) const {
                return a.f != b.f ? a.f > b.f : a.seq > b.seq;
            }
        };
        std::priority_queue<Node, std::vector<Node>, Cmp> open;

        const int startIdx = index(start.x, start.z);
        const int goalIdx = index(goal.x, goal.z);
        g[static_cast<size_t>(startIdx)] = 0.0f;
        uint32_t seq = 0;
        open.push({heuristic(start, goal), seq++, startIdx});

        static constexpr int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static constexpr int dz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        const float kDiag = 1.41421356f;

        while (!open.empty()) {
            const Node cur = open.top();
            open.pop();
            if (closed[static_cast<size_t>(cur.idx)]) {
                continue; // stale duplicate
            }
            if (cur.idx == goalIdx) {
                reconstruct(came, goalIdx, outPath);
                return true;
            }
            closed[static_cast<size_t>(cur.idx)] = 1;
            const int cx = cur.idx % m_w;
            const int cz = cur.idx / m_w;

            for (int d = 0; d < 8; ++d) {
                const int nx = cx + dx[d];
                const int nz = cz + dz[d];
                if (!walkable(nx, nz)) {
                    continue;
                }
                const bool diagonal = dx[d] != 0 && dz[d] != 0;
                if (diagonal) {
                    // Disallow squeezing diagonally between two solids (corner cutting).
                    if (!walkable(cx + dx[d], cz) || !walkable(cx, cz + dz[d])) {
                        continue;
                    }
                }
                const int ni = index(nx, nz);
                if (closed[static_cast<size_t>(ni)]) {
                    continue;
                }
                const float step = diagonal ? kDiag : 1.0f;
                const float tentative = g[static_cast<size_t>(cur.idx)] + step;
                if (tentative < g[static_cast<size_t>(ni)]) {
                    g[static_cast<size_t>(ni)] = tentative;
                    came[static_cast<size_t>(ni)] = cur.idx;
                    open.push({tentative + heuristic({nx, nz}, goal), seq++, ni});
                }
            }
        }
        return false;
    }

    // Convenience: A* between two world positions, returning the route as world-space cell centers.
    bool findWorldPath(const math::vec3& start, const math::vec3& goal,
                       std::vector<math::vec3>& outWaypoints) const {
        std::vector<Cell> cells;
        if (!findPath(worldToCell(start), worldToCell(goal), cells)) {
            outWaypoints.clear();
            return false;
        }
        outWaypoints.clear();
        outWaypoints.reserve(cells.size());
        for (const Cell& c : cells) {
            outWaypoints.push_back(cellToWorld(c));
        }
        return true;
    }

private:
    int index(int x, int z) const { return z * m_w + x; }

    // Octile distance: exact shortest 8-connected distance ignoring obstacles (admissible).
    float heuristic(const Cell& a, const Cell& b) const {
        const int adx = std::abs(a.x - b.x);
        const int adz = std::abs(a.z - b.z);
        const int hi = adx > adz ? adx : adz;
        const int lo = adx > adz ? adz : adx;
        return static_cast<float>(hi - lo) + 1.41421356f * static_cast<float>(lo);
    }

    void reconstruct(const std::vector<int32_t>& came, int goalIdx, std::vector<Cell>& out) const {
        out.clear();
        for (int i = goalIdx; i != -1; i = came[static_cast<size_t>(i)]) {
            out.push_back(Cell{i % m_w, i / m_w});
        }
        std::reverse(out.begin(), out.end());
    }

    int m_w = 0;
    int m_h = 0;
    float m_cell = 1.0f;
    math::vec3 m_origin{0.0f};
    std::vector<uint8_t> m_blocked;
};

} // namespace maz::game
