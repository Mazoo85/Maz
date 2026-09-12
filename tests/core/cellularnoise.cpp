// tests/core/cellularnoise.cpp — verifies Worley/cellular noise (core::worley2D / worley3D).
// Ground truths, all deterministic integer-hash + float math:
//   * determinism: the same (position, seed) always returns the same F1/F2;
//   * ordering: F1 <= F2 and both are non-negative everywhere sampled;
//   * feature points: sampling exactly at a cell's computed feature point gives F1 ~ 0;
//   * bound: F1 never exceeds ~sqrt(2) in 2D (a feature point always lives in the central cell);
//   * seed independence: different seeds produce different fields;
//   * the 3D variant satisfies the same ordering + bound (~sqrt(3)).
#include "maz/core/CellularNoise.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::CellularSample;
using maz::core::worley2D;
using maz::core::worley3D;
namespace d = maz::core::detail;

int main() {
    // --- 1. Determinism + ordering + non-negativity over a grid. ---
    {
        bool orderOk = true, nonNeg = true, deterministic = true, boundOk = true;
        for (int i = 0; i < 40; ++i) {
            const float x = static_cast<float>(i) * 0.37f - 5.0f;
            const float y = static_cast<float>(i) * 0.71f + 2.0f;
            const CellularSample a = worley2D(x, y, 1234U);
            const CellularSample b = worley2D(x, y, 1234U);
            if (a.f1 != b.f1 || a.f2 != b.f2) deterministic = false;
            if (a.f1 > a.f2 + 1e-6f) orderOk = false;
            if (a.f1 < 0.0f || a.f2 < 0.0f) nonNeg = false;
            if (a.f1 > 1.4143f) boundOk = false; // sqrt(2) + epsilon
        }
        CHECK(deterministic, "same input+seed is deterministic");
        CHECK(orderOk, "F1 <= F2 everywhere");
        CHECK(nonNeg, "F1, F2 >= 0");
        CHECK(boundOk, "F1 bounded by ~sqrt(2) in 2D");
    }

    // --- 2. Sampling at a cell's own feature point gives F1 ~ 0. ---
    {
        const uint32_t seed = 99U;
        // Reconstruct cell (0,0)'s feature point the way worley2D places it.
        const uint32_t h = d::hashCell(0, 0, seed);
        const float px = 0.0f + d::unit01(h);
        const float py = 0.0f + d::unit01(d::mix32(h));
        const CellularSample s = worley2D(px, py, seed);
        CHECK(s.f1 < 1e-5f, "F1 ~ 0 exactly at a feature point");
        CHECK(s.f2 > s.f1, "F2 is a genuinely different (farther) point");
    }

    // --- 3. Seed independence: at least one sample differs between seeds. ---
    {
        bool differs = false;
        for (int i = 0; i < 20 && !differs; ++i) {
            const float x = static_cast<float>(i) * 0.5f;
            const CellularSample a = worley2D(x, 0.25f, 1U);
            const CellularSample b = worley2D(x, 0.25f, 2U);
            if (std::fabs(a.f1 - b.f1) > 1e-4f) differs = true;
        }
        CHECK(differs, "different seeds give different fields");
    }

    // --- 4. F2 - F1 (the cell-edge signal) is non-negative and varies. ---
    {
        const float e0 = worley2D(0.5f, 0.5f, 7U).f2 - worley2D(0.5f, 0.5f, 7U).f1;
        const float e1 = worley2D(3.1f, 1.9f, 7U).f2 - worley2D(3.1f, 1.9f, 7U).f1;
        CHECK(e0 >= 0.0f && e1 >= 0.0f, "F2 - F1 >= 0 (usable edge field)");
        CHECK(std::fabs(e0 - e1) > 1e-6f, "edge field varies across space");
    }

    // --- 5. 3D variant: ordering + bound (~sqrt(3)). ---
    {
        bool orderOk = true, boundOk = true, deterministic = true;
        for (int i = 0; i < 30; ++i) {
            const float x = static_cast<float>(i) * 0.33f;
            const float y = static_cast<float>(i) * 0.61f - 3.0f;
            const float z = static_cast<float>(i) * 0.19f + 1.0f;
            const CellularSample a = worley3D(x, y, z, 55U);
            const CellularSample b = worley3D(x, y, z, 55U);
            if (a.f1 != b.f1) deterministic = false;
            if (a.f1 > a.f2 + 1e-6f) orderOk = false;
            if (a.f1 > 1.7321f) boundOk = false; // sqrt(3) + epsilon
        }
        CHECK(deterministic, "3D deterministic");
        CHECK(orderOk, "3D F1 <= F2");
        CHECK(boundOk, "3D F1 bounded by ~sqrt(3)");
        const CellularSample s = worley3D(2.5f, -1.5f, 0.5f, 55U);
        CHECK(s.f1 >= 0.0f, "3D F1 non-negative");
    }

    if (g_fail == 0) {
        std::printf("cellularnoise: OK — determinism, F1<=F2, feature-point zero, bounds, seed "
                    "independence, edge field, 3D.\n");
        return 0;
    }
    std::printf("cellularnoise: %d failure(s).\n", g_fail);
    return 1;
}
