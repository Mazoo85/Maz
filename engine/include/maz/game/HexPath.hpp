#pragma once

#include "maz/game/HexGrid.hpp" // Hex, hexNeighbors, hexDistance

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// maz::game hex A* pathfinding — shortest path across a hexagonal grid, built on HexGrid's axial
// coordinates. You supply a `blocked(hex)` predicate (walls / impassable terrain); the search walks
// the six neighbours with a hex-distance heuristic (admissible, so the result is a genuine shortest
// path) and returns the cell list from start to goal inclusive, or empty if unreachable. Godot's
// AStar2D makes you register every node and edge by hand; this is the ready-made hex pathfinder on
// top of the M280 coordinate algebra. Header-only, deterministic, exactly unit-testable.
namespace maz::game {

namespace detail {
struct HexHash {
    std::size_t operator()(const Hex& h) const {
        // Pack the two ints into 64 bits (offset avoids negatives colliding) and hash.
        const std::uint64_t k = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(h.q)) << 32) ^
                                static_cast<std::uint32_t>(h.r);
        return std::hash<std::uint64_t>()(k);
    }
};
} // namespace detail

// A* from `start` to `goal`. `blocked` returns true for impassable cells. `maxRange` bounds the
// search radius from the start (keeps an open/unbounded grid from expanding forever). Returns the
// path start..goal inclusive, or an empty vector if there is no route (incl. a blocked start/goal).
inline std::vector<Hex> hexFindPath(const Hex& start, const Hex& goal,
                                    const std::function<bool(const Hex&)>& blocked,
                                    int maxRange = 64) {
    if (blocked(start) || blocked(goal)) {
        return {};
    }
    if (start == goal) {
        return {start};
    }

    using detail::HexHash;
    std::unordered_map<Hex, int, HexHash> gScore;   // best known cost from start
    std::unordered_map<Hex, Hex, HexHash> cameFrom; // predecessor on the best path
    gScore[start] = 0;

    // Open set as a flat vector scanned for the lowest f — plenty for the bounded ranges hex maps use.
    std::vector<Hex> open;
    open.push_back(start);

    auto heuristic = [&](const Hex& h) { return hexDistance(h, goal); };

    while (!open.empty()) {
        // Pick the open node with the smallest f = g + h.
        std::size_t bestIdx = 0;
        int bestF = gScore[open[0]] + heuristic(open[0]);
        for (std::size_t i = 1; i < open.size(); ++i) {
            const int f = gScore[open[i]] + heuristic(open[i]);
            if (f < bestF) {
                bestF = f;
                bestIdx = i;
            }
        }
        const Hex current = open[bestIdx];
        if (current == goal) {
            std::vector<Hex> path;
            Hex node = goal;
            path.push_back(node);
            while (node != start) {
                node = cameFrom[node];
                path.push_back(node);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        open[bestIdx] = open.back();
        open.pop_back();

        const int g = gScore[current];
        for (const Hex& n : hexNeighbors(current)) {
            if (blocked(n) || hexDistance(start, n) > maxRange) {
                continue;
            }
            const int tentative = g + 1;
            const auto it = gScore.find(n);
            if (it == gScore.end() || tentative < it->second) {
                gScore[n] = tentative;
                cameFrom[n] = current;
                open.push_back(n); // may re-add; the g-check above keeps it correct
            }
        }
    }
    return {}; // unreachable
}

} // namespace maz::game
