// tests/game/diamondsquare.cpp — verifies the diamond-square heightmap generator (game DiamondSquare.hpp).
// Ground truths, deterministic:
//   * the grid side is 2^exponent + 1 and every height is finite;
//   * amplitude 0 -> the whole map equals the baseline (pure corner interpolation of equal corners);
//   * the same seed reproduces an identical map; a different seed differs somewhere;
//   * higher roughness (slower amplitude decay) yields a rougher map — larger height variance;
//   * exponent is clamped to a sane range.
#include "maz/game/DiamondSquare.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::game::DiamondSquare;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static double variance(const DiamondSquare::HeightMap& m) {
    double mean = 0.0;
    for (float v : m.h) mean += v;
    mean /= static_cast<double>(m.h.size());
    double s = 0.0;
    for (float v : m.h) {
        const double d = static_cast<double>(v) - mean;
        s += d * d;
    }
    return s / static_cast<double>(m.h.size());
}

int main() {
    // --- 1. Size + finiteness. ---
    {
        const DiamondSquare::HeightMap m = DiamondSquare::generate(6, 12345u, 0.5f, 1.0f);
        CHECK(m.size == 65, "exponent 6 -> side 2^6 + 1 = 65");
        CHECK(m.h.size() == 65u * 65u, "grid has size*size cells");
        bool finite = true;
        for (float v : m.h) if (!std::isfinite(v)) finite = false;
        CHECK(finite, "all heights are finite");
    }

    // --- 2. Amplitude 0 -> flat at baseline. ---
    {
        const DiamondSquare::HeightMap m = DiamondSquare::generate(5, 7u, 0.6f, 0.0f, 3.5f);
        bool flat = true;
        for (float v : m.h) if (std::fabs(v - 3.5f) > 1e-5f) flat = false;
        CHECK(flat, "zero amplitude yields a flat map at the baseline");
    }

    // --- 3. Determinism. ---
    {
        const DiamondSquare::HeightMap a = DiamondSquare::generate(6, 999u, 0.5f, 1.0f);
        const DiamondSquare::HeightMap b = DiamondSquare::generate(6, 999u, 0.5f, 1.0f);
        bool same = a.h.size() == b.h.size();
        for (std::size_t i = 0; same && i < a.h.size(); ++i)
            if (a.h[i] != b.h[i]) same = false;
        CHECK(same, "same seed reproduces an identical map");

        const DiamondSquare::HeightMap c = DiamondSquare::generate(6, 1000u, 0.5f, 1.0f);
        bool differs = false;
        for (std::size_t i = 0; i < a.h.size(); ++i)
            if (a.h[i] != c.h[i]) { differs = true; break; }
        CHECK(differs, "a different seed produces a different map");
    }

    // --- 4. Higher roughness -> larger variance. ---
    {
        const DiamondSquare::HeightMap smooth = DiamondSquare::generate(7, 42u, 0.35f, 1.0f);
        const DiamondSquare::HeightMap rough = DiamondSquare::generate(7, 42u, 0.85f, 1.0f);
        CHECK(variance(rough) > variance(smooth), "higher roughness gives a bumpier (higher-variance) map");
    }

    // --- 5. Exponent clamping. ---
    {
        const DiamondSquare::HeightMap lo = DiamondSquare::generate(-3, 1u, 0.5f, 1.0f);
        CHECK(lo.size == 2, "negative exponent clamps to side 2 (2^0 + 1)");
        const DiamondSquare::HeightMap hi = DiamondSquare::generate(99, 1u, 0.5f, 1.0f);
        CHECK(hi.size == (1 << 12) + 1, "huge exponent clamps to 2^12 + 1");
    }

    if (g_fail == 0) {
        std::printf("diamondsquare: OK — size, flat baseline, determinism, roughness variance, clamp.\n");
        return 0;
    }
    std::printf("diamondsquare: %d failure(s).\n", g_fail);
    return 1;
}
