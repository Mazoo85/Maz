#pragma once

#include <cmath>
#include <cstdint>

// maz::core Worley (cellular / "Voronoi") noise — the procedural-texture staple the engine's Perlin
// noise (core::Noise) doesn't cover. Space is divided into unit cells, each holding one deterministic
// feature point; sampling a position returns F1 (distance to the nearest feature point) and F2 (to the
// second nearest). F1 alone gives bubbly/organic cells (think water caustics, cracked mud, cell walls);
// F2−F1 traces the ridges *between* cells (stone veins, crackle, Voronoi edges). Unlike Voronoi.hpp
// (which builds an explicit Delaunay/Voronoi diagram from a point set), this is a cheap, seed-driven,
// continuously-sampleable NOISE FIELD with no allocation — a hash places each cell's point, so it is
// deterministic and unit-tests exactly.
namespace maz::core {

namespace detail {
// A good integer bit-mixer (fmix-style) → uniform-ish 32-bit hash.
inline uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline uint32_t hashCell(int cx, int cy, uint32_t seed) {
    uint32_t h = seed * 0x9e3779b1U;
    h = mix32(h ^ (static_cast<uint32_t>(cx) * 0x85ebca77U));
    h = mix32(h ^ (static_cast<uint32_t>(cy) * 0xc2b2ae3dU));
    return mix32(h);
}
inline uint32_t hashCell3(int cx, int cy, int cz, uint32_t seed) {
    uint32_t h = hashCell(cx, cy, seed);
    h = mix32(h ^ (static_cast<uint32_t>(cz) * 0x27d4eb2fU));
    return h;
}
inline float unit01(uint32_t h) { return static_cast<float>(h) / 4294967296.0f; } // [0,1)
inline int floorInt(float v) { return static_cast<int>(std::floor(v)); }
} // namespace detail

// The two nearest feature-point distances at a sample position. f1 <= f2 always.
struct CellularSample {
    float f1 = 0.0f; // distance to the nearest feature point
    float f2 = 0.0f; // distance to the second nearest
};

// 2D Worley noise. Searches the 3x3 block of cells around the sample so the true two nearest points
// are always found (a point in the center cell is at most ~1.41 away, and any closer point lives in a
// neighbouring cell). `seed` selects an independent field.
inline CellularSample worley2D(float x, float y, uint32_t seed = 0) {
    const int xi = detail::floorInt(x);
    const int yi = detail::floorInt(y);
    float f1 = 1e30f;
    float f2 = 1e30f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int cx = xi + dx;
            const int cy = yi + dy;
            const uint32_t h = detail::hashCell(cx, cy, seed);
            const float px = static_cast<float>(cx) + detail::unit01(h);
            const float py = static_cast<float>(cy) + detail::unit01(detail::mix32(h));
            const float ex = px - x;
            const float ey = py - y;
            const float d = std::sqrt(ex * ex + ey * ey);
            if (d < f1) {
                f2 = f1;
                f1 = d;
            } else if (d < f2) {
                f2 = d;
            }
        }
    }
    return CellularSample{f1, f2};
}

// 3D Worley noise (searches the 3x3x3 block of surrounding cells).
inline CellularSample worley3D(float x, float y, float z, uint32_t seed = 0) {
    const int xi = detail::floorInt(x);
    const int yi = detail::floorInt(y);
    const int zi = detail::floorInt(z);
    float f1 = 1e30f;
    float f2 = 1e30f;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = xi + dx;
                const int cy = yi + dy;
                const int cz = zi + dz;
                const uint32_t h = detail::hashCell3(cx, cy, cz, seed);
                const float px = static_cast<float>(cx) + detail::unit01(h);
                const float py = static_cast<float>(cy) + detail::unit01(detail::mix32(h));
                const float pz = static_cast<float>(cz) + detail::unit01(detail::mix32(h + 1U));
                const float ex = px - x;
                const float ey = py - y;
                const float ez = pz - z;
                const float d = std::sqrt(ex * ex + ey * ey + ez * ez);
                if (d < f1) {
                    f2 = f1;
                    f1 = d;
                } else if (d < f2) {
                    f2 = d;
                }
            }
        }
    }
    return CellularSample{f1, f2};
}

} // namespace maz::core
