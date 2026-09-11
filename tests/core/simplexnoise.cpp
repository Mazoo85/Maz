// tests/core/simplexnoise.cpp — verifies 2D Simplex noise (core::simplex2D / simplexFbm2D).
// Ground truths (Simplex has no exact lattice values, so these are structural, deterministic):
//   * determinism per (position, seed);
//   * range ~[-1,1]: over a dense grid |n| stays under ~1.05, and the field actually swings (peak > 0.5);
//   * continuity: two nearby samples differ by a small amount (bounded gradient, no discontinuities);
//   * near-zero mean over a large region (balanced positive/negative);
//   * seed independence: different seeds give different fields;
//   * fBm stays within range and is deterministic.
#include "maz/core/SimplexNoise.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::simplex2D;
using maz::core::simplexFbm2D;

int main() {
    // --- 1. Range, amplitude, zero-mean over a dense grid. ---
    {
        float maxAbs = 0.0f;
        float peak = 0.0f;
        double sum = 0.0;
        long count = 0;
        for (int a = 0; a < 400; ++a) {
            for (int b = 0; b < 400; ++b) {
                const float x = static_cast<float>(a) * 0.113f;
                const float y = static_cast<float>(b) * 0.087f;
                const float n = simplex2D(x, y, 7u);
                maxAbs = std::max(maxAbs, std::fabs(n));
                peak = std::max(peak, n);
                sum += static_cast<double>(n);
                ++count;
            }
        }
        CHECK(maxAbs <= 1.05f, "|simplex| stays within ~1.05 (approx [-1,1])");
        CHECK(peak > 0.5f, "field has real amplitude (peak > 0.5)");
        const double mean = sum / static_cast<double>(count);
        CHECK(std::fabs(mean) < 0.05, "near-zero mean over a large region");
    }

    // --- 2. Determinism. ---
    {
        CHECK(simplex2D(1.5f, -2.25f, 3u) == simplex2D(1.5f, -2.25f, 3u), "deterministic");
    }

    // --- 3. Continuity: nearby samples are close. ---
    {
        bool smooth = true;
        for (int k = 0; k < 200; ++k) {
            const float x = static_cast<float>(k) * 0.031f;
            const float y = static_cast<float>(k) * 0.047f + 0.5f;
            const float a = simplex2D(x, y, 11u);
            const float b = simplex2D(x + 0.001f, y + 0.001f, 11u);
            if (std::fabs(a - b) > 0.05f) smooth = false; // bounded gradient
        }
        CHECK(smooth, "noise is continuous (small step -> small change)");
    }

    // --- 4. Seed independence. ---
    {
        bool differs = false;
        for (int k = 0; k < 50 && !differs; ++k) {
            const float x = static_cast<float>(k) * 0.3f;
            if (std::fabs(simplex2D(x, 0.5f, 1u) - simplex2D(x, 0.5f, 2u)) > 1e-3f) differs = true;
        }
        CHECK(differs, "different seeds give different fields");
    }

    // --- 5. fBm: range + determinism. ---
    {
        float maxAbs = 0.0f;
        for (int a = 0; a < 100; ++a) {
            for (int b = 0; b < 100; ++b) {
                const float n = simplexFbm2D(static_cast<float>(a) * 0.05f,
                                             static_cast<float>(b) * 0.05f, 5, 2.0f, 0.5f, 9u);
                maxAbs = std::max(maxAbs, std::fabs(n));
            }
        }
        CHECK(maxAbs <= 1.05f, "fBm stays within range");
        CHECK(simplexFbm2D(1.1f, 2.2f, 4, 2.0f, 0.5f, 9u) ==
                  simplexFbm2D(1.1f, 2.2f, 4, 2.0f, 0.5f, 9u),
              "fBm deterministic");
    }

    if (g_fail == 0) {
        std::printf("simplexnoise: OK — range/amplitude/mean, determinism, continuity, seed "
                    "independence, fBm.\n");
        return 0;
    }
    std::printf("simplexnoise: %d failure(s).\n", g_fail);
    return 1;
}
