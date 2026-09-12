#pragma once

#include "maz/math/VectorInt.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

// maz::game Dijkstra map — the Brogue-style scalar "desire map" that drives roguelike monster AI.
// Multi-source breadth-first search fills every passable cell with its step-distance to the NEAREST of
// one or more goals (walls impassable); a monster then just walks to the lowest-valued neighbour to
// approach (`descend`). Negating and re-scanning the map (`makeFleeMap`) turns "walk toward the hero"
// into "walk away from the hero", and several maps can be combined for richer behaviour (safety,
// desire, patrol). This complements the vector-field FlowField (M112) — that bakes a single-goal
// float integration field into per-cell direction vectors for crowds; this is a multi-source INTEGER
// distance field the game reads and recombines directly, the shape roguelikes actually use. Unit-cost
// BFS (4- or 8-connected), deterministic, header-only. Godot leaves this to the game.
namespace maz::game {

using MapCell = math::Vector2i;

struct DijkstraMap {
    static constexpr int kUnreachable = 1 << 30;
    int width = 0;
    int height = 0;
    std::vector<int> dist; // step distance to nearest goal, kUnreachable where blocked/unreachable

    int at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return kUnreachable;
        }
        return dist[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x)];
    }

    // The neighbour (4- or 8-connected) with the strictly lowest value — the step a pursuer takes.
    // Returns `from` unchanged when no neighbour is lower (a goal, a local sink, or fully walled in).
    MapCell descend(MapCell from, bool diagonal = false) const {
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        const int n = diagonal ? 8 : 4;
        MapCell best = from;
        int bestVal = at(from.x, from.y);
        for (int i = 0; i < n; ++i) {
            const int nx = from.x + dx[i];
            const int ny = from.y + dy[i];
            const int v = at(nx, ny);
            if (v < bestVal) {
                bestVal = v;
                best = MapCell(nx, ny);
            }
        }
        return best;
    }
};

// Fill a `width` x `height` map with the step-distance from every passable cell to the nearest goal.
// `blocked(cell)` marks walls; goals on walls are ignored. 4-connected by default (8 if `diagonal`).
inline DijkstraMap buildDijkstraMap(int width, int height, const std::vector<MapCell>& goals,
                                    const std::function<bool(const MapCell&)>& blocked,
                                    bool diagonal = false) {
    DijkstraMap m;
    m.width = std::max(0, width);
    m.height = std::max(0, height);
    m.dist.assign(static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height),
                  DijkstraMap::kUnreachable);
    if (m.width == 0 || m.height == 0) {
        return m;
    }
    auto idx = [&](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m.width) +
               static_cast<std::size_t>(x);
    };
    std::vector<MapCell> frontier;
    std::vector<MapCell> next;
    for (const MapCell& g : goals) {
        if (g.x < 0 || g.y < 0 || g.x >= m.width || g.y >= m.height) {
            continue;
        }
        if (blocked(g)) {
            continue;
        }
        if (m.dist[idx(g.x, g.y)] != 0) {
            m.dist[idx(g.x, g.y)] = 0;
            frontier.push_back(g);
        }
    }
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int nn = diagonal ? 8 : 4;
    int d = 1;
    while (!frontier.empty()) {
        next.clear();
        for (const MapCell& c : frontier) {
            for (int i = 0; i < nn; ++i) {
                const int nx = c.x + dx[i];
                const int ny = c.y + dy[i];
                if (nx < 0 || ny < 0 || nx >= m.width || ny >= m.height) {
                    continue;
                }
                if (m.dist[idx(nx, ny)] != DijkstraMap::kUnreachable) {
                    continue;
                }
                const MapCell nc(nx, ny);
                if (blocked(nc)) {
                    continue;
                }
                m.dist[idx(nx, ny)] = d;
                next.push_back(nc);
            }
        }
        frontier.swap(next);
        ++d;
    }
    return m;
}

// Turn an approach map into a flee map (Brogue's trick): scale every reachable value by `coeff` (a
// negative number, classically about -1.2 so fleeing beats hiding in a dead end), then relax to a
// fixpoint so the whole field descends AWAY from the original goals. Descending the result walks a
// pursuer to safety. Cells unreachable in `approach` stay unreachable.
inline DijkstraMap makeFleeMap(const DijkstraMap& approach,
                               const std::function<bool(const MapCell&)>& blocked,
                               float coeff = -1.2f, bool diagonal = false) {
    DijkstraMap m;
    m.width = approach.width;
    m.height = approach.height;
    m.dist.assign(static_cast<std::size_t>(m.width) * static_cast<std::size_t>(m.height),
                  DijkstraMap::kUnreachable);
    if (m.width == 0 || m.height == 0) {
        return m;
    }
    auto idx = [&](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m.width) +
               static_cast<std::size_t>(x);
    };
    for (int y = 0; y < m.height; ++y) {
        for (int x = 0; x < m.width; ++x) {
            const int a = approach.dist[idx(x, y)];
            if (a != DijkstraMap::kUnreachable) {
                m.dist[idx(x, y)] = static_cast<int>(std::lround(static_cast<float>(a) * coeff));
            }
        }
    }
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int nn = diagonal ? 8 : 4;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 0; y < m.height; ++y) {
            for (int x = 0; x < m.width; ++x) {
                if (m.dist[idx(x, y)] == DijkstraMap::kUnreachable) {
                    continue;
                }
                if (blocked(MapCell(x, y))) {
                    continue;
                }
                int lowest = DijkstraMap::kUnreachable;
                for (int i = 0; i < nn; ++i) {
                    const int nx = x + dx[i];
                    const int ny = y + dy[i];
                    if (nx < 0 || ny < 0 || nx >= m.width || ny >= m.height) {
                        continue;
                    }
                    lowest = std::min(lowest, m.dist[idx(nx, ny)]);
                }
                if (lowest != DijkstraMap::kUnreachable && lowest + 1 < m.dist[idx(x, y)]) {
                    m.dist[idx(x, y)] = lowest + 1;
                    changed = true;
                }
            }
        }
    }
    return m;
}

} // namespace maz::game
