#pragma once

// Ken Perlin's improved gradient noise (2D/3D) plus fBm, seeded via maz::core::Rng
// so a given seed reproduces the same field across runs/platforms. Output is roughly
// in [-1, 1] and is exactly 0 at integer lattice coordinates. The Godot FastNoiseLite
// analog for procedural generation; composes iter4-era maz::core::Rng for the
// permutation table. noise2D is the z=0 slice of noise3D (one code path). NOT
// thread-safe for construction; const sampling is pure/reentrant. Simplex/cellular/
// domain-warp and a 3D fBm are future refinements.

#include "maz/core/Random.hpp"
#include "maz/core/Assert.hpp"

#include <array>
#include <cstdint>
#include <cmath>

namespace maz::math {

class PerlinNoise {
public:
    explicit PerlinNoise(std::uint64_t seed = 0) {
        std::array<int, 256> p{};
        for (int i = 0; i < 256; ++i) {
            p[static_cast<std::size_t>(i)] = i;
        }
        maz::core::Rng rng(seed);
        for (int i = 255; i > 0; --i) {
            int j = static_cast<int>(rng.nextU32() % static_cast<std::uint32_t>(i + 1));
            std::swap(p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>(j)]);
        }
        for (int i = 0; i < 512; ++i) {
            m_perm[static_cast<std::size_t>(i)] = p[static_cast<std::size_t>(i & 255)];
        }
    }

    // Classic improved Perlin noise. Output ~[-1, 1]; exactly 0 at integer lattice
    // coordinates (fractional parts are 0 -> the fade weights select the origin
    // corner whose offset is (0,0,0) -> 0).
    float noise3D(float x, float y, float z) const {
        const int X = static_cast<int>(std::floor(x)) & 255;
        const int Y = static_cast<int>(std::floor(y)) & 255;
        const int Z = static_cast<int>(std::floor(z)) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);
        const float u = fade(x), v = fade(y), w = fade(z);
        const int A = perm(X) + Y, AA = perm(A) + Z, AB = perm(A + 1) + Z;
        const int B = perm(X + 1) + Y, BA = perm(B) + Z, BB = perm(B + 1) + Z;
        return lerp(
            lerp(lerp(grad(perm(AA), x, y, z),
                      grad(perm(BA), x - 1.0f, y, z), u),
                 lerp(grad(perm(AB), x, y - 1.0f, z),
                      grad(perm(BB), x - 1.0f, y - 1.0f, z), u), v),
            lerp(lerp(grad(perm(AA + 1), x, y, z - 1.0f),
                      grad(perm(BA + 1), x - 1.0f, y, z - 1.0f), u),
                 lerp(grad(perm(AB + 1), x, y - 1.0f, z - 1.0f),
                      grad(perm(BB + 1), x - 1.0f, y - 1.0f, z - 1.0f), u), v), w);
    }

    // The z=0 slice of 3D Perlin is a valid 2D noise; exactly 0 at integer (x, y).
    float noise2D(float x, float y) const { return noise3D(x, y, 0.0f); }

    // Fractal Brownian motion: sum octaves of noise2D at increasing frequency and
    // decreasing amplitude. For octaves == 1 this returns exactly noise2D(x, y).
    float fbm2D(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const {
        MAZ_ASSERT(octaves >= 1, "PerlinNoise::fbm2D needs >= 1 octave");
        float sum = 0.0f, amp = 1.0f, freq = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * noise2D(x * freq, y * freq);
            freq *= lacunarity;
            amp *= gain;
        }
        return sum;
    }

private:
    // Perlin's quintic smoothstep 6t^5 - 15t^4 + 10t^3 (zero 1st & 2nd derivatives
    // at 0 and 1).
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    // Local lerp (kept standalone so this header does not depend on math lerp).
    static float lerp(float a, float b, float t) { return a + t * (b - a); }

    // Ken Perlin's improved-noise gradient (the standard 16-case form).
    static float grad(int hash, float x, float y, float z) {
        const int h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    int perm(int i) const { return m_perm[static_cast<std::size_t>(i)]; }

    std::array<int, 512> m_perm; // permutation of 0..255, duplicated to 512
};

} // namespace maz::math
