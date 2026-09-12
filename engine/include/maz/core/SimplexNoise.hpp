#pragma once

#include <cmath>
#include <cstdint>

// maz::core 2D Simplex noise — the gradient noise that complements the engine's Perlin (core::Noise)
// and Worley (core::CellularNoise). Simplex noise (Ken Perlin's successor to classic Perlin noise)
// tiles space with triangles rather than a square grid, which removes the axis-aligned directional
// artifacts Perlin can show, evaluates with fewer multiplies, and has well-defined continuous
// gradients — the usual default for terrain height, clouds, flow maps, and organic textures (it's what
// FastNoiseLite exposes as its default). This is a seedable, hash-gradient implementation (no big
// permutation table) following Gustavson's reference construction; output is approximately [-1, 1].
// Pure float math, no allocation, deterministic — unit-tested by its range, continuity, zero-ish mean,
// and determinism (Simplex has no exact lattice values to check, unlike a radical inverse).
namespace maz::core {

namespace detail {
inline uint32_t simplexMix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline uint32_t simplexHash(int i, int j, uint32_t seed) {
    uint32_t h = seed * 0x9e3779b1U;
    h = simplexMix(h ^ (static_cast<uint32_t>(i) * 0x85ebca77U));
    h = simplexMix(h ^ (static_cast<uint32_t>(j) * 0xc2b2ae3dU));
    return simplexMix(h);
}
// 8-direction gradient set (4 diagonals + 4 axes), selected by the low 3 bits of the hash.
inline float simplexGrad(uint32_t h, float x, float y) {
    switch (h & 7u) {
        case 0: return x + y;
        case 1: return -x + y;
        case 2: return x - y;
        case 3: return -x - y;
        case 4: return x;
        case 5: return -x;
        case 6: return y;
        default: return -y;
    }
}
} // namespace detail

// 2D Simplex noise at (x, y). `seed` selects an independent field. Range is approximately [-1, 1].
inline float simplex2D(float x, float y, uint32_t seed = 0) {
    const float F2 = 0.3660254037844386f;   // 0.5 * (sqrt(3) - 1)
    const float G2 = 0.21132486540518713f;  // (3 - sqrt(3)) / 6

    // Skew the input space to determine which simplex (triangle) cell we're in.
    const float s = (x + y) * F2;
    const int i = static_cast<int>(std::floor(x + s));
    const int j = static_cast<int>(std::floor(y + s));
    const float t = static_cast<float>(i + j) * G2;
    const float x0 = x - (static_cast<float>(i) - t); // unskewed distance from cell origin
    const float y0 = y - (static_cast<float>(j) - t);

    // Which of the two triangles of the cell are we in?
    int i1 = 0;
    int j1 = 0;
    if (x0 > y0) {
        i1 = 1;
    } else {
        j1 = 1;
    }

    const float x1 = x0 - static_cast<float>(i1) + G2;
    const float y1 = y0 - static_cast<float>(j1) + G2;
    const float x2 = x0 - 1.0f + 2.0f * G2;
    const float y2 = y0 - 1.0f + 2.0f * G2;

    float n0 = 0.0f;
    float n1 = 0.0f;
    float n2 = 0.0f;
    float u0 = 0.5f - x0 * x0 - y0 * y0;
    if (u0 > 0.0f) {
        u0 *= u0;
        n0 = u0 * u0 * detail::simplexGrad(detail::simplexHash(i, j, seed), x0, y0);
    }
    float u1 = 0.5f - x1 * x1 - y1 * y1;
    if (u1 > 0.0f) {
        u1 *= u1;
        n1 = u1 * u1 * detail::simplexGrad(detail::simplexHash(i + i1, j + j1, seed), x1, y1);
    }
    float u2 = 0.5f - x2 * x2 - y2 * y2;
    if (u2 > 0.0f) {
        u2 *= u2;
        n2 = u2 * u2 * detail::simplexGrad(detail::simplexHash(i + 1, j + 1, seed), x2, y2);
    }
    // Scale to roughly [-1, 1] (empirically ~70 for this 8-gradient set: raw peak ~0.0143 -> ~1.0).
    return 70.0f * (n0 + n1 + n2);
}

// Fractal (fBm) Simplex: sum of octaves at doubling frequency and halving amplitude, normalized to
// approximately [-1, 1].
inline float simplexFbm2D(float x, float y, int octaves, float lacunarity = 2.0f,
                          float gain = 0.5f, uint32_t seed = 0) {
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * simplex2D(x * freq, y * freq, seed + static_cast<uint32_t>(o) * 131u);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

} // namespace maz::core
