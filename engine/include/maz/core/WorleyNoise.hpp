#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

// maz::core::WorleyNoise — cellular / "Worley" noise: scatter one jittered feature point per unit grid
// cell, then for any sample point return the distance to the nearest feature point (F1) and the
// second-nearest (F2). F1 gives a field of rounded cell blobs (stone, scales, cracked mud, water
// caustics, bubbles); F2 - F1 traces the ridges BETWEEN cells (crack/vein networks, Voronoi edges).
// This is the scalar-texture cousin of the engine's geometric Voronoi diagram and complements the
// Perlin `Noise` (M85), which cannot make cellular patterns. Deterministic from a seed, tileable-free,
// header-only, std-only. (Reaches parity with Godot's FastNoiseLite cellular mode, which Maz's own
// Noise module previously lacked.)
namespace maz::core {

class WorleyNoise {
public:
    explicit WorleyNoise(std::uint32_t seed = 0) : m_seed(seed) {}

    // Distance to the nearest feature point.
    float f1(float x, float y) const { return f1f2(x, y).first; }

    // {F1, F2} = distances to the nearest and second-nearest feature points (F2 >= F1 >= 0).
    std::pair<float, float> f1f2(float x, float y) const {
        const int xi = floorInt(x);
        const int yi = floorInt(y);
        float best1 = 1.0e30f;
        float best2 = 1.0e30f;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = xi + dx;
                const int cy = yi + dy;
                const float fx = static_cast<float>(cx) + hashf(cx, cy, 0);
                const float fy = static_cast<float>(cy) + hashf(cx, cy, 1);
                const float ex = x - fx;
                const float ey = y - fy;
                const float d = std::sqrt(ex * ex + ey * ey);
                if (d < best1) {
                    best2 = best1;
                    best1 = d;
                } else if (d < best2) {
                    best2 = d;
                }
            }
        }
        return {best1, best2};
    }

    // F2 - F1: near 0 on cell edges (the Voronoi boundaries), larger toward cell interiors — invert for
    // crack/vein networks.
    float crackle(float x, float y) const {
        const std::pair<float, float> p = f1f2(x, y);
        return p.second - p.first;
    }

private:
    static int floorInt(float v) {
        const int i = static_cast<int>(v);
        return (v < static_cast<float>(i)) ? i - 1 : i;
    }

    // Deterministic hash of (cellX, cellY, channel, seed) -> [0, 1) jitter offset.
    float hashf(int cx, int cy, int channel) const {
        std::uint32_t h = static_cast<std::uint32_t>(cx) * 374761393u +
                          static_cast<std::uint32_t>(cy) * 668265263u +
                          static_cast<std::uint32_t>(channel) * 2246822519u + m_seed * 362437u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000);
    }

    std::uint32_t m_seed;
};

} // namespace maz::core
