// tests/game/erosion.cpp — verifies thermal erosion (game Erosion.hpp).
// Ground truths, deterministic (no <random>, no clock — seeded LCG for terrain):
//   * MASS CONSERVATION: the sum of all heights is unchanged (material only moves between cells);
//   * SLOPE RELAXATION: the steepest adjacent height difference drops well below its starting value;
//   * a single tall spike in flat terrain spreads to its neighbours (spike shrinks, neighbours rise);
//   * terrain already flatter than the talus angle is left untouched;
//   * erosion is deterministic.
#include "maz/game/Erosion.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::thermalErosion;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

static double sum(const std::vector<float>& v) {
    double a = 0.0;
    for (float x : v) a += x;
    return a;
}

static float maxSlope(const std::vector<float>& hgt, int w, int h) {
    float m = 0.0f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float c = hgt[static_cast<std::size_t>(y * w + x)];
            if (x + 1 < w) m = std::max(m, std::fabs(c - hgt[static_cast<std::size_t>(y * w + x + 1)]));
            if (y + 1 < h) m = std::max(m, std::fabs(c - hgt[static_cast<std::size_t>((y + 1) * w + x)]));
        }
    return m;
}

int main() {
    // --- 1. Mass conservation + slope relaxation on rough terrain. ---
    {
        const int w = 40, h = 40;
        std::vector<float> hgt(static_cast<std::size_t>(w * h));
        Lcg rng{0xE1051Eu};
        for (auto& v : hgt) v = rng.unit() * 100.0f; // very jagged
        const double massBefore = sum(hgt);
        const float slopeBefore = maxSlope(hgt, w, h);

        thermalErosion(hgt, w, h, 4.0f, 80, 0.5f);

        const double massAfter = sum(hgt);
        const float slopeAfter = maxSlope(hgt, w, h);
        CHECK(std::fabs(massAfter - massBefore) < 1e-2, "total height (mass) is conserved");
        CHECK(slopeAfter < slopeBefore * 0.5f, "the steepest slope relaxes substantially");
    }

    // --- 2. A spike spreads to its neighbours. ---
    {
        const int w = 5, h = 5;
        std::vector<float> hgt(25, 0.0f);
        hgt[static_cast<std::size_t>(2 * 5 + 2)] = 100.0f; // center spike
        const double before = sum(hgt);
        thermalErosion(hgt, w, h, 1.0f, 50, 0.5f);
        CHECK(std::fabs(sum(hgt) - before) < 1e-3, "spike test conserves mass");
        CHECK(hgt[static_cast<std::size_t>(2 * 5 + 2)] < 100.0f, "the spike loses height");
        const float neigh = hgt[static_cast<std::size_t>(2 * 5 + 1)] + hgt[static_cast<std::size_t>(2 * 5 + 3)] +
                            hgt[static_cast<std::size_t>(1 * 5 + 2)] + hgt[static_cast<std::size_t>(3 * 5 + 2)];
        CHECK(neigh > 0.0f, "neighbours receive the slid material");
    }

    // --- 3. Gentle terrain (below talus) is untouched. ---
    {
        const int w = 8, h = 8;
        std::vector<float> hgt(64);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                hgt[static_cast<std::size_t>(y * w + x)] = static_cast<float>(x) * 0.5f; // slope 0.5/cell
        const std::vector<float> before = hgt;
        thermalErosion(hgt, w, h, 1.0f, 50, 0.5f); // talus 1.0 > slope 0.5 -> nothing moves
        bool same = true;
        for (std::size_t i = 0; i < hgt.size(); ++i)
            if (std::fabs(hgt[i] - before[i]) > 1e-6f) same = false;
        CHECK(same, "terrain below the talus angle is left unchanged");
    }

    // --- 4. Determinism. ---
    {
        const int w = 16, h = 16;
        std::vector<float> a(static_cast<std::size_t>(w * h)), b;
        Lcg rng{0x5EEDu};
        for (auto& v : a) v = rng.unit() * 50.0f;
        b = a;
        thermalErosion(a, w, h, 2.0f, 30);
        thermalErosion(b, w, h, 2.0f, 30);
        CHECK(a == b, "erosion is deterministic");
    }

    if (g_fail == 0) {
        std::printf("erosion: OK — mass conservation, slope relaxation, spike spread, no-op, determinism.\n");
        return 0;
    }
    std::printf("erosion: %d failure(s).\n", g_fail);
    return 1;
}
