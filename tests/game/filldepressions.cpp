// tests/game/filldepressions.cpp — verifies priority-flood depression filling (game FillDepressions.hpp).
// Ground truths, deterministic (seeded LCG for terrains, no <random>, no clock):
//   * output >= input everywhere (filling only raises land) and the boundary is untouched;
//   * NO interior pit remains: every interior cell has a neighbour at <= its filled height (a downhill exit);
//   * an analytic bowl fills up to its outlet level, not higher;
//   * idempotence: filling an already-filled surface changes nothing; determinism.
#include "maz/game/FillDepressions.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

int main() {
    // --- 1. Invariants over random terrains. ---
    {
        Lcg rng{0xD3E7A1u};
        bool raiseOk = true, borderOk = true, noPit = true;
        int trials = 0;
        for (int t = 0; t < 300 && raiseOk && borderOk && noPit; ++t) {
            const int w = 5 + static_cast<int>(rng.next() % 12u);
            const int h = 5 + static_cast<int>(rng.next() % 12u);
            std::vector<float> e(static_cast<std::size_t>(w * h));
            for (float& v : e) v = rng.range(0.0f, 10.0f);
            const std::vector<float> f = maz::game::fillDepressions(w, h, e);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const std::size_t idx = static_cast<std::size_t>(y * w + x);
                    if (f[idx] < e[idx] - 1e-5f) raiseOk = false;
                    const bool border = x == 0 || y == 0 || x == w - 1 || y == h - 1;
                    if (border && std::fabs(f[idx] - e[idx]) > 1e-5f) borderOk = false;
                    if (!border) {
                        // At least one neighbour is <= this cell's filled height (water can leave).
                        float mn = 1e30f;
                        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
                        for (int d = 0; d < 4; ++d) {
                            const std::size_t n = static_cast<std::size_t>((y + dy[d]) * w + (x + dx[d]));
                            mn = std::fmin(mn, f[n]);
                        }
                        if (mn > f[idx] + 1e-4f) noPit = false; // strict interior pit -> fail
                    }
                }
            ++trials;
        }
        CHECK(trials > 200, "the depression-fill battery ran");
        CHECK(raiseOk, "filled surface is >= the input everywhere");
        CHECK(borderOk, "the boundary is left unchanged");
        CHECK(noPit, "no interior pit remains (every cell has a non-ascending exit)");
    }

    // --- 2. Analytic bowl: fills up to the outlet, not higher. ---
    {
        const int w = 5, h = 5;
        std::vector<float> e(static_cast<std::size_t>(w * h), 8.0f); // walls at 8
        // Interior 3x3 is a pit at 0.
        for (int y = 1; y <= 3; ++y)
            for (int x = 1; x <= 3; ++x) e[static_cast<std::size_t>(y * w + x)] = 0.0f;
        // One border outlet at height 3.
        e[static_cast<std::size_t>(2 * w + 0)] = 3.0f;
        const std::vector<float> f = maz::game::fillDepressions(w, h, e);
        bool filledToOutlet = true;
        for (int y = 1; y <= 3; ++y)
            for (int x = 1; x <= 3; ++x)
                if (std::fabs(f[static_cast<std::size_t>(y * w + x)] - 3.0f) > 1e-4f) filledToOutlet = false;
        CHECK(filledToOutlet, "the basin fills exactly to its lowest outlet level (3)");
        CHECK(std::fabs(f[static_cast<std::size_t>(2 * w + 0)] - 3.0f) < 1e-4f, "the outlet cell is unchanged");
    }

    // --- 3. Idempotence + determinism. ---
    {
        Lcg rng{0x4242u};
        const int w = 10, h = 8;
        std::vector<float> e(static_cast<std::size_t>(w * h));
        for (float& v : e) v = rng.range(0.0f, 5.0f);
        const std::vector<float> f1 = maz::game::fillDepressions(w, h, e);
        const std::vector<float> f2 = maz::game::fillDepressions(w, h, f1);
        CHECK(f1 == f2, "filling an already-filled surface changes nothing (idempotent)");
        const std::vector<float> f1b = maz::game::fillDepressions(w, h, e);
        CHECK(f1 == f1b, "deterministic output");
    }

    if (g_fail == 0) {
        std::printf("filldepressions: OK — raise+border invariants, no-pit, analytic bowl, idempotence.\n");
        return 0;
    }
    std::printf("filldepressions: %d failure(s).\n", g_fail);
    return 1;
}
