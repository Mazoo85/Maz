#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::game::DiamondSquare — the diamond-square (midpoint-displacement) fractal heightmap generator. It
// produces a (2^n + 1) square grid of heights by seeding the four corners and repeatedly subdividing:
// each "diamond" step sets a square's centre to the average of its four corners plus a random offset, each
// "square" step sets an edge midpoint to the average of its orthogonal neighbours plus a random offset, and
// the random amplitude shrinks each level (roughness). The result is self-similar fractal terrain with a
// characteristic ridged/plasma look — distinct from the engine's Perlin/fbm noise (core::Noise): where
// value/gradient noise is band-limited and smooth, diamond-square is a recursive random-midpoint fractal,
// the classic generator for island heightmaps, cloud/plasma textures, and lightning. Deterministic given a
// seed (embedded splitmix64 — no <random>, no clock), header-only, std-only. Godot ships no such generator.
namespace maz::game {

class DiamondSquare {
public:
    // A generated heightmap: a `size` x `size` grid (size == 2^exponent + 1), row-major.
    struct HeightMap {
        int size = 0;
        std::vector<float> h;
        float at(int x, int y) const {
            return h[static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                     static_cast<std::size_t>(x)];
        }
    };

    // exponent >= 0 -> grid side 2^exponent + 1 (clamped to [1,12]). seed drives the deterministic RNG.
    // roughness in (0,1] scales how fast the random amplitude decays per level (higher = rougher/more
    // detail). amplitude is the initial random magnitude. baseline is the (shared) corner height; with
    // amplitude 0 the whole map equals baseline.
    static HeightMap generate(int exponent, std::uint64_t seed, float roughness, float amplitude,
                              float baseline = 0.0f) {
        if (exponent < 0) {
            exponent = 0;
        }
        if (exponent > 12) {
            exponent = 12;
        }
        const int size = (1 << exponent) + 1;
        HeightMap map;
        map.size = size;
        map.h.assign(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), baseline);

        std::uint64_t rngState = seed + 0x9E3779B97F4A7C15ull;
        auto next = [&rngState]() -> std::uint64_t {
            rngState += 0x9E3779B97F4A7C15ull;
            std::uint64_t z = rngState;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        };
        auto rnd = [&next]() -> float { // uniform in [-1, 1)
            return static_cast<float>(next() >> 11) * (1.0f / 9007199254740992.0f) * 2.0f - 1.0f;
        };

        auto get = [&](int x, int y) -> float {
            return map.h[static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                         static_cast<std::size_t>(x)];
        };
        auto put = [&](int x, int y, float v) {
            map.h[static_cast<std::size_t>(y) * static_cast<std::size_t>(size) +
                  static_cast<std::size_t>(x)] = v;
        };

        // Seed the four corners (already baseline; add jitter so a non-flat map varies at the top level).
        put(0, 0, baseline + rnd() * amplitude);
        put(size - 1, 0, baseline + rnd() * amplitude);
        put(0, size - 1, baseline + rnd() * amplitude);
        put(size - 1, size - 1, baseline + rnd() * amplitude);

        float amp = amplitude;
        for (int step = size - 1; step > 1; step /= 2) {
            const int half = step / 2;

            // Diamond step: square centres = average of the 4 diagonal corners + jitter.
            for (int y = half; y < size; y += step) {
                for (int x = half; x < size; x += step) {
                    const float avg = (get(x - half, y - half) + get(x + half, y - half) +
                                       get(x - half, y + half) + get(x + half, y + half)) *
                                      0.25f;
                    put(x, y, avg + rnd() * amp);
                }
            }

            // Square step: edge midpoints = average of orthogonal neighbours (in-bounds) + jitter.
            for (int y = 0; y < size; y += half) {
                for (int x = (y + half) % step; x < size; x += step) {
                    float sum = 0.0f;
                    int count = 0;
                    if (x - half >= 0) { sum += get(x - half, y); ++count; }
                    if (x + half < size) { sum += get(x + half, y); ++count; }
                    if (y - half >= 0) { sum += get(x, y - half); ++count; }
                    if (y + half < size) { sum += get(x, y + half); ++count; }
                    const float avg = count > 0 ? sum / static_cast<float>(count) : baseline;
                    put(x, y, avg + rnd() * amp);
                }
            }

            amp *= roughness;
        }

        return map;
    }
};

} // namespace maz::game
