// tests/net/floatquant.cpp — verifies bounded float quantization (net FloatQuant.hpp).
// Ground truths, deterministic:
//   * the range endpoints quantize to 0 and 2^bits-1 and round-trip exactly;
//   * any value round-trips to within half a quantization step;
//   * values outside [min,max] clamp;
//   * more bits give smaller round-trip error;
//   * the quantized code never exceeds its bit width;
//   * angles are periodic: -pi and +pi share a code, and an angle round-trips within half a step.
#include "maz/net/FloatQuant.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::net::dequantizeAngle;
using maz::net::dequantizeFloat;
using maz::net::quantizeAngle;
using maz::net::quantizeFloat;

static bool near(float a, float b, float e) { return std::fabs(a - b) < e; }

int main() {
    const float mn = -1000.0f, mx = 1000.0f;

    // --- 1. Endpoints exact. ---
    {
        const int bits = 16;
        CHECK(quantizeFloat(mn, mn, mx, bits) == 0u, "min quantizes to 0");
        CHECK(quantizeFloat(mx, mn, mx, bits) == (1u << bits) - 1u, "max quantizes to 2^bits-1");
        CHECK(near(dequantizeFloat(quantizeFloat(mn, mn, mx, bits), mn, mx, bits), mn, 1e-2f),
              "min round-trips exactly");
        CHECK(near(dequantizeFloat(quantizeFloat(mx, mn, mx, bits), mn, mx, bits), mx, 1e-2f),
              "max round-trips exactly");
    }

    // --- 2. Round trip within half a step. ---
    {
        const int bits = 12;
        const float step = (mx - mn) / static_cast<float>((1u << bits) - 1u);
        bool ok = true;
        for (float v = mn; v <= mx; v += 37.0f) {
            const float r = dequantizeFloat(quantizeFloat(v, mn, mx, bits), mn, mx, bits);
            if (!near(r, v, step * 0.5f + 1e-3f)) ok = false;
        }
        CHECK(ok, "every value round-trips within half a quantization step");
    }

    // --- 3. Clamping. ---
    {
        const int bits = 10;
        CHECK(quantizeFloat(mn - 500.0f, mn, mx, bits) == 0u, "below min clamps to 0");
        CHECK(quantizeFloat(mx + 500.0f, mn, mx, bits) == (1u << bits) - 1u, "above max clamps to top");
    }

    // --- 4. More bits -> less error. ---
    {
        const float v = 123.456f;
        const float e8 = std::fabs(dequantizeFloat(quantizeFloat(v, mn, mx, 8), mn, mx, 8) - v);
        const float e16 = std::fabs(dequantizeFloat(quantizeFloat(v, mn, mx, 16), mn, mx, 16) - v);
        CHECK(e16 < e8, "16-bit quantization is more accurate than 8-bit");
    }

    // --- 5. Code fits in its bit width. ---
    {
        const int bits = 11;
        bool fits = true;
        for (float v = mn; v <= mx; v += 13.0f)
            if (quantizeFloat(v, mn, mx, bits) >= (1u << bits)) fits = false;
        CHECK(fits, "quantized code never exceeds 2^bits-1");
    }

    // --- 6. Angle periodicity + round trip. ---
    {
        const int bits = 12;
        const float pi = 3.14159265f;
        // -pi and +pi are the same direction -> same code.
        CHECK(quantizeAngle(-pi, bits) == quantizeAngle(pi, bits), "-pi and +pi share a code (no seam)");
        const float astep = 6.2831853f / static_cast<float>(1u << bits);
        bool ok = true;
        for (float a = -pi; a <= pi; a += 0.19f) {
            const float r = dequantizeAngle(quantizeAngle(a, bits), bits);
            // Compare on the circle (shortest angular difference).
            float d = std::fabs(r - a);
            while (d > pi) d = std::fabs(d - 6.2831853f);
            if (d > astep * 0.5f + 1e-3f) ok = false;
        }
        CHECK(ok, "angles round-trip within half a step (on the circle)");
    }

    if (g_fail == 0) {
        std::printf("floatquant: OK — endpoints, half-step round trip, clamp, bit-depth error, code "
                    "width, angle periodicity.\n");
        return 0;
    }
    std::printf("floatquant: %d failure(s).\n", g_fail);
    return 1;
}
