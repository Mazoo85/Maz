#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

// maz::game::fillDepressions — priority-flood depression filling for heightfields (Barnes, Lehman & Mulla
// 2014). Terrain from noise or erosion is riddled with pits and closed basins where water would pool and get
// stuck with nowhere to drain. Before you can trace rivers, compute drainage / flow accumulation, place
// lakes, or run a hydraulic-erosion pass, you first "fill" every depression up to the lowest lip over which
// water could spill — turning the surface into one where every point has a downhill (non-ascending) path to
// the map edge. This is the standard hydrology preprocessing step (ArcGIS "Fill", GRASS r.fill.dir); Godot
// has none. The algorithm floods inward from the boundary using a min-priority queue keyed by spill height,
// so each interior cell is raised to the maximum of its own height and the lowest level needed to reach the
// edge. Output >= input everywhere, the boundary is untouched, and no interior pit remains. Header-only,
// std-only, deterministic (the filled surface is unique regardless of tie order).
namespace maz::game {

// Fill depressions in a `width` x `height` elevation grid (row-major). Returns the filled elevation.
inline std::vector<float> fillDepressions(int width, int height, const std::vector<float>& elev) {
    std::vector<float> filled = elev;
    if (width < 3 || height < 3 ||
        elev.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return filled;
    }
    const std::size_t w = static_cast<std::size_t>(width);
    std::vector<std::uint8_t> visited(elev.size(), 0);

    struct Cell {
        float level;
        int idx;
        bool operator>(const Cell& o) const { return level > o.level; }
    };
    std::priority_queue<Cell, std::vector<Cell>, std::greater<Cell>> pq;

    auto push = [&](int x, int y) {
        const std::size_t idx = static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
        if (!visited[idx]) {
            visited[idx] = 1;
            pq.push(Cell{filled[idx], static_cast<int>(idx)});
        }
    };
    // Seed the boundary (natural outlets) at their own height.
    for (int x = 0; x < width; ++x) {
        push(x, 0);
        push(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        push(0, y);
        push(width - 1, y);
    }

    const int dx[4] = {-1, 1, 0, 0};
    const int dy[4] = {0, 0, -1, 1};
    while (!pq.empty()) {
        const Cell c = pq.top();
        pq.pop();
        const int cx = static_cast<int>(static_cast<std::size_t>(c.idx) % w);
        const int cy = static_cast<int>(static_cast<std::size_t>(c.idx) / w);
        for (int d = 0; d < 4; ++d) {
            const int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                continue;
            }
            const std::size_t nidx = static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx);
            if (visited[nidx]) {
                continue;
            }
            visited[nidx] = 1;
            // Raise the neighbour to at least the current spill level.
            if (filled[nidx] < c.level) {
                filled[nidx] = c.level;
            }
            pq.push(Cell{filled[nidx], static_cast<int>(nidx)});
        }
    }
    return filled;
}

} // namespace maz::game
