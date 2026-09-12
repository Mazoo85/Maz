#pragma once

#include "maz/core/Random.hpp"

#include <cmath>
#include <cstdint>

namespace maz::core {

// Procedural noise: smooth, seeded, reproducible pseudo-randomness over space — the primitive behind
// terrain heightmaps, cloud/marble textures, cave carving, and organic motion. This is classic Perlin
// gradient noise: a per-seed permutation table (shuffled with core::Random, so the same seed always
// gives the same field) plus fade/lerp interpolation of lattice gradients, yielding a value in about
// [-1, 1] that is 0 at integer lattice points and continuous everywhere. fbm2 layers octaves of it
// (fractal Brownian motion) for natural detail, normalized back into [-1, 1]. Header-only.

class Noise {
public:
    Noise() { seed(0); }
    explicit Noise(uint64_t s) { seed(s); }

    // Build the permutation table from a seed (Fisher-Yates over 0..255, duplicated to 512).
    void seed(uint64_t s) {
        for (int i = 0; i < 256; ++i) m_p[i] = static_cast<uint8_t>(i);
        Random rng(s);
        for (int i = 255; i > 0; --i) {
            const int j = rng.range(0, i);
            const uint8_t tmp = m_p[i];
            m_p[i] = m_p[j];
            m_p[j] = tmp;
        }
        for (int i = 0; i < 512; ++i) m_perm[i] = m_p[i & 255];
    }

    // 2D Perlin noise, output ~[-1, 1]; exactly 0 at integer (x, y).
    float noise2(float x, float y) const {
        const float fx = std::floor(x), fy = std::floor(y);
        const int X = static_cast<int>(fx) & 255;
        const int Y = static_cast<int>(fy) & 255;
        const float xf = x - fx, yf = y - fy;
        const float u = fade(xf), v = fade(yf);
        const int aa = m_perm[static_cast<size_t>(m_perm[static_cast<size_t>(X)] + Y)];
        const int ab = m_perm[static_cast<size_t>(m_perm[static_cast<size_t>(X)] + Y + 1)];
        const int ba = m_perm[static_cast<size_t>(m_perm[static_cast<size_t>(X + 1)] + Y)];
        const int bb = m_perm[static_cast<size_t>(m_perm[static_cast<size_t>(X + 1)] + Y + 1)];
        const float x1 = lerp(grad(static_cast<uint8_t>(aa), xf, yf),
                              grad(static_cast<uint8_t>(ba), xf - 1.0f, yf), u);
        const float x2 = lerp(grad(static_cast<uint8_t>(ab), xf, yf - 1.0f),
                              grad(static_cast<uint8_t>(bb), xf - 1.0f, yf - 1.0f), u);
        return lerp(x1, x2, v);
    }

    // Fractal Brownian motion: `octaves` layers of noise at rising frequency (lacunarity) and falling
    // amplitude (gain), normalized so the result stays ~[-1, 1].
    float fbm2(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const {
        float amp = 1.0f, freq = 1.0f, sum = 0.0f, norm = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * noise2(x * freq, y * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

private:
    uint8_t m_p[256];
    uint8_t m_perm[512];

    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }
    static float grad(uint8_t h, float x, float y) {
        // 8 gradient directions (axis + diagonal) selected by the low bits of the hash.
        switch (h & 7) {
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
};

} // namespace maz::core
