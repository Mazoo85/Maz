// tests/math/lowdiscrepancy.cpp — verifies the low-discrepancy sequences (math::radicalInverse,
// halton2D, hammersley2D). Ground truths, exact/known-value math, deterministic:
//   * base-2 radical inverse matches its textbook values (1/2, 1/4, 3/4, 1/8, 5/8, 3/8, 7/8);
//   * base-3 radical inverse matches (1/3, 2/3, 1/9);
//   * all outputs lie in [0,1);
//   * STRATIFICATION: the first 8 base-2 points (i=0..7), sorted, are exactly {0/8..7/8} — perfectly
//     even, the defining low-discrepancy property (a pseudo-random set would clump);
//   * Halton (2,3) fills the unit square with far smaller max-gap than the index alone;
//   * Hammersley x-axis is exactly i/count.
#include "maz/math/LowDiscrepancy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::halton2D;
using maz::math::hammersley2D;
using maz::math::radicalInverse;
using maz::math::vec2;

static bool near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Base-2 radical inverse known values. ---
    {
        CHECK(near(radicalInverse(2, 1), 0.5f), "radInv2(1) = 1/2");
        CHECK(near(radicalInverse(2, 2), 0.25f), "radInv2(2) = 1/4");
        CHECK(near(radicalInverse(2, 3), 0.75f), "radInv2(3) = 3/4");
        CHECK(near(radicalInverse(2, 4), 0.125f), "radInv2(4) = 1/8");
        CHECK(near(radicalInverse(2, 5), 0.625f), "radInv2(5) = 5/8");
        CHECK(near(radicalInverse(2, 6), 0.375f), "radInv2(6) = 3/8");
        CHECK(near(radicalInverse(2, 7), 0.875f), "radInv2(7) = 7/8");
        CHECK(radicalInverse(2, 0) == 0.0f, "radInv2(0) = 0");
    }

    // --- 2. Base-3 radical inverse. ---
    {
        CHECK(near(radicalInverse(3, 1), 1.0f / 3.0f), "radInv3(1) = 1/3");
        CHECK(near(radicalInverse(3, 2), 2.0f / 3.0f), "radInv3(2) = 2/3");
        CHECK(near(radicalInverse(3, 3), 1.0f / 9.0f), "radInv3(3) = 1/9");
    }

    // --- 3. Range [0,1). ---
    {
        bool inRange = true;
        for (uint32_t i = 0; i < 500; ++i) {
            const float v = radicalInverse(2, i);
            if (v < 0.0f || v >= 1.0f) inRange = false;
            const vec2 h = halton2D(i);
            if (h.x < 0.0f || h.x >= 1.0f || h.y < 0.0f || h.y >= 1.0f) inRange = false;
        }
        CHECK(inRange, "all outputs in [0,1)");
    }

    // --- 4. Stratification: first 8 base-2 points sorted are exactly k/8. ---
    {
        std::vector<float> pts;
        for (uint32_t i = 0; i < 8; ++i) {
            pts.push_back(radicalInverse(2, i));
        }
        std::sort(pts.begin(), pts.end());
        bool exact = true;
        for (int k = 0; k < 8; ++k) {
            if (!near(pts[static_cast<size_t>(k)], static_cast<float>(k) / 8.0f)) exact = false;
        }
        CHECK(exact, "first 8 base-2 points are perfectly stratified (k/8)");
    }

    // --- 5. Halton (2,3) has a small max gap over the first 64 points (well-spread). ---
    {
        std::vector<float> xs;
        for (uint32_t i = 0; i < 64; ++i) {
            xs.push_back(halton2D(i).x);
        }
        std::sort(xs.begin(), xs.end());
        float maxGap = xs.front(); // gap from 0
        for (size_t k = 1; k < xs.size(); ++k) {
            maxGap = std::max(maxGap, xs[k] - xs[k - 1]);
        }
        maxGap = std::max(maxGap, 1.0f - xs.back()); // gap to 1
        // 64 base-2 points are exactly multiples of 1/64, so the largest gap is 1/64.
        CHECK(maxGap < 0.02f, "Halton x-axis is densely, evenly spread (max gap < 1/50)");
    }

    // --- 6. Hammersley x = i/count exactly. ---
    {
        const uint32_t n = 16;
        bool ok = true;
        for (uint32_t i = 0; i < n; ++i) {
            const vec2 h = hammersley2D(i, n);
            if (!near(h.x, static_cast<float>(i) / static_cast<float>(n))) ok = false;
            if (!near(h.y, radicalInverse(2, i))) ok = false;
        }
        CHECK(ok, "Hammersley x = i/count, y = radInv2(i)");
    }

    if (g_fail == 0) {
        std::printf("lowdiscrepancy: OK — radical inverse (base 2/3), range, stratification, Halton "
                    "spread, Hammersley.\n");
        return 0;
    }
    std::printf("lowdiscrepancy: %d failure(s).\n", g_fail);
    return 1;
}
