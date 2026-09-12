#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

// maz::game D8 flow accumulation — figure out where water DRAINS on a heightmap: for each cell, how many
// cells upstream ultimately flow through it. This is the standard hydrology primitive that turns a terrain
// into rivers: high accumulation traces out valleys, streams, and river mouths, which drives procedural
// river placement, moisture/biome maps (wetter downstream), where hydraulic erosion cuts channels, and where
// to spawn lakes or settlements. The "D8" model routes each cell's water to its single STEEPEST-DESCENT
// neighbour among the 8 around it (or nowhere, if it is a local pit / map-edge outlet); accumulation is then
// the count of cells whose drainage passes through each cell (itself included). Because every cell's unit of
// water flows monotonically downhill until it leaves the map or reaches a pit, the total water is conserved —
// the accumulations at all the outlet cells sum to the cell count. Computed in one height-sorted pass, no
// recursion. Header-only, std-only, deterministic. Godot has no hydrology tools.
namespace maz::game {

struct FlowResult {
    std::vector<int> downstream;         // per cell: index of the cell it drains to, or -1 (outlet/pit)
    std::vector<std::uint32_t> accum;    // per cell: number of cells draining through it (>= 1)
};

// Compute D8 flow directions and accumulation for a width*height row-major heightmap.
inline FlowResult flowAccumulation(const std::vector<float>& height, int width, int height_) {
    FlowResult out;
    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height_);
    if (width <= 0 || height_ <= 0 || height.size() != n) {
        return out;
    }
    const int w = width;
    const int h = height_;
    out.downstream.assign(n, -1);
    out.accum.assign(n, 1u); // each cell contributes its own unit of water

    const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const float kSqrt2 = 1.41421356f;
    const float dist[8] = {1, 1, 1, 1, kSqrt2, kSqrt2, kSqrt2, kSqrt2};

    // Steepest-descent neighbour (largest drop per unit distance) for each cell.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t c = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                  static_cast<std::size_t>(x);
            const float hc = height[c];
            float bestSlope = 0.0f;
            int best = -1;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + dx[k];
                const int ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                    continue;
                }
                const std::size_t ni = static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) +
                                       static_cast<std::size_t>(nx);
                const float slope = (hc - height[ni]) / dist[k];
                if (slope > bestSlope) {
                    bestSlope = slope;
                    best = static_cast<int>(ni);
                }
            }
            out.downstream[c] = best; // -1 if no lower neighbour (pit or edge sink)
        }
    }

    // Push accumulation downhill in order of DECREASING height so a cell is finalised before its receiver.
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [&height](std::size_t a, std::size_t b) { return height[a] > height[b]; });
    for (std::size_t c : order) {
        const int d = out.downstream[c];
        if (d >= 0) {
            out.accum[static_cast<std::size_t>(d)] += out.accum[c];
        }
    }
    return out;
}

} // namespace maz::game
