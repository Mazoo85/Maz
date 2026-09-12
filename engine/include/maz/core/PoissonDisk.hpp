#pragma once

#include "maz/core/Pcg32.hpp" // core::Pcg32
#include "maz/math/Math.hpp"  // math::vec2

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

// maz::core Poisson-disk sampling (Bridson's algorithm) — "blue noise" point scatter where no two
// points are closer than a minimum radius, yet coverage stays dense and even. It looks far more natural
// than uniform-random scatter (which clumps) for placing grass, trees, rocks, spawn points, stipple
// dots, or sampling positions. Godot has no built-in blue-noise sampler, so this is a beyond-parity
// extra. Bridson runs in O(n) using a background grid for the neighbour check. Deterministic given a
// seed (uses core::Pcg32). Header-only, pure — unit-tests exactly (min-distance + in-bounds).
namespace maz::core {

// Sample points in the rectangle [min,max] with every pair at least `radius` apart. `k` is the number
// of candidate attempts per active point (Bridson uses 30). Deterministic for a given `seed`.
inline std::vector<math::vec2> poissonDiskSample(math::vec2 boundsMin, math::vec2 boundsMax,
                                                 float radius, std::uint64_t seed = 0, int k = 30) {
    std::vector<math::vec2> out;
    const float w = boundsMax.x - boundsMin.x;
    const float h = boundsMax.y - boundsMin.y;
    if (w <= 0.0f || h <= 0.0f || radius <= 0.0f) {
        return out;
    }

    // Background grid: cell size r/sqrt(2) so each cell holds at most one sample.
    const float cell = radius / 1.41421356f;
    const int gw = static_cast<int>(std::ceil(w / cell));
    const int gh = static_cast<int>(std::ceil(h / cell));
    std::vector<int> grid(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), -1);
    auto gridIndex = [&](const math::vec2& p) {
        int cx = static_cast<int>((p.x - boundsMin.x) / cell);
        int cy = static_cast<int>((p.y - boundsMin.y) / cell);
        if (cx < 0) cx = 0;
        if (cy < 0) cy = 0;
        if (cx >= gw) cx = gw - 1;
        if (cy >= gh) cy = gh - 1;
        return std::pair<int, int>(cx, cy);
    };

    Pcg32 rng(seed, 0xda3e39cb94b95bdbULL);
    const float r2 = radius * radius;
    auto farEnough = [&](const math::vec2& p) {
        auto [cx, cy] = gridIndex(p);
        for (int y = cy - 2; y <= cy + 2; ++y) {
            for (int x = cx - 2; x <= cx + 2; ++x) {
                if (x < 0 || y < 0 || x >= gw || y >= gh) {
                    continue;
                }
                const int idx = grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(gw) +
                                     static_cast<std::size_t>(x)];
                if (idx < 0) {
                    continue;
                }
                const math::vec2& q = out[static_cast<std::size_t>(idx)];
                const float dx = q.x - p.x;
                const float dy = q.y - p.y;
                if (dx * dx + dy * dy < r2) {
                    return false;
                }
            }
        }
        return true;
    };
    auto emit = [&](const math::vec2& p) {
        auto [cx, cy] = gridIndex(p);
        grid[static_cast<std::size_t>(cy) * static_cast<std::size_t>(gw) +
             static_cast<std::size_t>(cx)] = static_cast<int>(out.size());
        out.push_back(p);
    };

    // Seed with one point near the centre.
    std::vector<int> active;
    const math::vec2 first(boundsMin.x + w * 0.5f, boundsMin.y + h * 0.5f);
    emit(first);
    active.push_back(0);

    const float twoPi = 6.28318530718f;
    while (!active.empty()) {
        const int ai = static_cast<int>(rng.nextBounded(static_cast<std::uint32_t>(active.size())));
        const math::vec2 center = out[static_cast<std::size_t>(active[static_cast<std::size_t>(ai)])];
        bool found = false;
        for (int i = 0; i < k; ++i) {
            const float ang = rng.nextFloat() * twoPi;
            const float rad = radius * (1.0f + rng.nextFloat()); // annulus [r, 2r)
            const math::vec2 cand(center.x + std::cos(ang) * rad, center.y + std::sin(ang) * rad);
            if (cand.x < boundsMin.x || cand.y < boundsMin.y || cand.x >= boundsMax.x ||
                cand.y >= boundsMax.y) {
                continue;
            }
            if (farEnough(cand)) {
                emit(cand);
                active.push_back(static_cast<int>(out.size()) - 1);
                found = true;
                break;
            }
        }
        if (!found) {
            // Retire this active point (swap-remove).
            active[static_cast<std::size_t>(ai)] = active.back();
            active.pop_back();
        }
    }
    return out;
}

} // namespace maz::core
