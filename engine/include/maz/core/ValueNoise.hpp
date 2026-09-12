#pragma once

#include <cmath>
#include <cstdint>

// maz::core::ValueNoise — classic *value* noise, the sibling of the Perlin gradient noise in
// core/Noise.hpp. Where Perlin interpolates random *gradients* attached to lattice points, value
// noise interpolates random *values* stored at the lattice points themselves. It is cheaper, has a
// slightly blockier/rounder character (no zero-crossings forced through the lattice), and is the
// classic ingredient for clouds, soft terrain, and cheap organic textures. This mirrors Godot's
// FastNoiseLite TYPE_VALUE (quintic-smoothstep interpolation) and TYPE_VALUE_CUBIC (Catmull-Rom
// bicubic interpolation) noise types, which the engine did not previously have in any form.
//
// A self-contained integer hash maps each (ix, iy, seed) lattice cell to a reproducible value in
// [-1, 1], so the same seed always yields the same field with no shared state. value2 is bilinear
// with a quintic fade and stays strictly within [-1, 1]; value2Cubic is bicubic (Catmull-Rom) and
// is smoother but may overshoot the input range slightly, as cubic interpolating splines do. Both
// are *interpolating*: at integer lattice coordinates they return the stored lattice value exactly.
// fbm2 layers octaves of value2 into fractal Brownian motion, normalized back to ~[-1, 1].
// Header-only, std-only.
namespace maz::core {

class ValueNoise {
public:
    ValueNoise() = default;
    explicit ValueNoise(uint32_t s) : m_seed(s) {}

    void seed(uint32_t s) { m_seed = s; }
    uint32_t seed() const { return m_seed; }

    // Reproducible value at an integer lattice cell, in [-1, 1].
    float lattice(int ix, int iy) const {
        uint32_t h = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iy) * 668265263u
                     + m_seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return static_cast<float>(h) * (2.0f / 4294967295.0f) - 1.0f;
    }

    // Bilinear value noise with a quintic fade. Output strictly within [-1, 1]; equals lattice(x, y)
    // exactly at integer coordinates.
    float value2(float x, float y) const {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const float tx = fade(x - fx);
        const float ty = fade(y - fy);
        const float v00 = lattice(x0, y0);
        const float v10 = lattice(x0 + 1, y0);
        const float v01 = lattice(x0, y0 + 1);
        const float v11 = lattice(x0 + 1, y0 + 1);
        return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), ty);
    }

    // Bicubic (Catmull-Rom) value noise over the 4x4 neighbourhood. Smoother than value2 and passes
    // exactly through the lattice values, but may slightly overshoot [-1, 1] like any cubic spline.
    float value2Cubic(float x, float y) const {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const int x0 = static_cast<int>(fx);
        const int y0 = static_cast<int>(fy);
        const float tx = x - fx;
        const float ty = y - fy;
        float rows[4];
        for (int j = 0; j < 4; ++j) {
            const int yy = y0 - 1 + j;
            rows[j] = cubic(lattice(x0 - 1, yy), lattice(x0, yy), lattice(x0 + 1, yy),
                            lattice(x0 + 2, yy), tx);
        }
        return cubic(rows[0], rows[1], rows[2], rows[3], ty);
    }

    // Fractal Brownian motion: `octaves` layers of value2 at rising frequency and falling amplitude,
    // normalized so the result stays ~[-1, 1].
    float fbm2(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const {
        float amp = 1.0f;
        float freq = 1.0f;
        float sum = 0.0f;
        float norm = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * value2(x * freq, y * freq);
            norm += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return norm > 0.0f ? sum / norm : 0.0f;
    }

private:
    uint32_t m_seed = 0;

    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }

    // Catmull-Rom cubic through b (t=0) and c (t=1), using a and d as neighbouring tangents.
    static float cubic(float a, float b, float c, float d, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f
               * (2.0f * b + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2
                  + (-a + 3.0f * b - 3.0f * c + d) * t3);
    }
};

} // namespace maz::core
