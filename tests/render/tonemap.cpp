// tests/render/tonemap.cpp — verifies the tonemapping operators (render::acesFilmic / reinhard /
// reinhardExtended / uncharted2). Ground truths, exact/continuous float math, deterministic:
//   * every operator maps 0 -> 0 and is monotonically increasing;
//   * output stays in [0,1] for all non-negative HDR input;
//   * ACES compresses a moderate value below 1 and saturates a very bright value to 1;
//   * reinhard(1) == 0.5 and reinhard approaches 1 for large input;
//   * reinhardExtended maps its white point to (nearly) 1;
//   * per-channel ACES applies the scalar curve independently to r/g/b.
#include "maz/render/Tonemap.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::Color3;
using maz::render::acesFilmic;
using maz::render::reinhard;
using maz::render::reinhardExtended;
using maz::render::uncharted2;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Maps 0 -> 0. ---
    {
        CHECK(near(acesFilmic(0.0f), 0.0f), "ACES(0) = 0");
        CHECK(near(reinhard(0.0f), 0.0f), "reinhard(0) = 0");
        CHECK(near(uncharted2(0.0f), 0.0f), "uncharted2(0) = 0");
    }

    // --- 2. Monotonic increasing + range [0,1] over a sweep. ---
    {
        float prevA = -1.0f, prevR = -1.0f, prevU = -1.0f;
        bool monoA = true, monoR = true, monoU = true, ranged = true;
        for (int k = 0; k <= 200; ++k) {
            const float x = static_cast<float>(k) * 0.1f; // 0 .. 20
            const float a = acesFilmic(x);
            const float r = reinhard(x);
            const float u = uncharted2(x);
            if (a < prevA - 1e-6f) monoA = false;
            if (r < prevR - 1e-6f) monoR = false;
            if (u < prevU - 1e-6f) monoU = false;
            if (a < 0.0f || a > 1.0f || r < 0.0f || r > 1.0f || u < 0.0f || u > 1.0f) ranged = false;
            prevA = a; prevR = r; prevU = u;
        }
        CHECK(monoA && monoR && monoU, "all operators monotonically increasing");
        CHECK(ranged, "all outputs in [0,1]");
    }

    // --- 3. ACES: compresses midtones, saturates highlights. ---
    {
        const float mid = acesFilmic(0.5f);
        CHECK(mid > 0.0f && mid < 1.0f, "ACES(0.5) is a compressed midtone in (0,1)");
        CHECK(near(acesFilmic(100.0f), 1.0f, 1e-3f), "ACES saturates a very bright value to 1");
    }

    // --- 4. Reinhard known values. ---
    {
        CHECK(near(reinhard(1.0f), 0.5f), "reinhard(1) = 0.5");
        CHECK(reinhard(1000.0f) > 0.999f, "reinhard approaches 1 for large input");
    }

    // --- 5. Extended Reinhard maps the white point to ~1. ---
    {
        const float w = 4.0f;
        CHECK(reinhardExtended(w, w) > 0.99f, "reinhardExtended(white, white) ~ 1");
        CHECK(reinhardExtended(0.0f, w) == 0.0f, "reinhardExtended(0) = 0");
    }

    // --- 6. Per-channel ACES. ---
    {
        const Color3 in{0.5f, 2.0f, 10.0f};
        const Color3 out = acesFilmic(in);
        CHECK(near(out.r, acesFilmic(0.5f)) && near(out.g, acesFilmic(2.0f)) &&
                  near(out.b, acesFilmic(10.0f)),
              "per-channel ACES applies the scalar curve to each channel");
        CHECK(out.r < out.g && out.g < out.b, "brighter input channels map to brighter outputs");
    }

    if (g_fail == 0) {
        std::printf("tonemap: OK — zero, monotonic, range, ACES compress/saturate, reinhard values, "
                    "extended white, per-channel.\n");
        return 0;
    }
    std::printf("tonemap: %d failure(s).\n", g_fail);
    return 1;
}
