// tests/core/summedareatable.cpp — verifies the 2D prefix-sum table (core SummedAreaTable.hpp).
// Ground truths, deterministic:
//   * rectangle sums match a brute-force double loop for a known small grid and for many random rectangles
//     over a random grid;
//   * single-cell, full-grid, and swapped/out-of-order corners behave;
//   * rectMean equals sum/count; a constant-time box blur (rectMean per pixel) matches a brute-force blur;
//   * an empty/mismatched grid is invalid and returns 0.
#include "maz/core/SummedAreaTable.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::core::SummedAreaTable;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(double a, double b, double e = 1e-6) { return std::fabs(a - b) < e; }

// Brute-force inclusive rectangle sum over a row-major grid.
static double bruteSum(const std::vector<double>& g, int w, int x0, int y0, int x1, int y1) {
    double s = 0.0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            s += g[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                    static_cast<std::size_t>(x)];
    return s;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

int main() {
    // --- 1. Known small grid. ---
    {
        // 3x2 grid:
        //   1 2 3
        //   4 5 6
        const std::vector<double> g{1, 2, 3, 4, 5, 6};
        SummedAreaTable<double> sat(g, 3, 2);
        CHECK(sat.valid() && sat.width() == 3 && sat.height() == 2, "built 3x2");
        CHECK(near(sat.total(), 21.0), "total = 21");
        CHECK(near(sat.rectSum(0, 0, 2, 1), 21.0), "full-rect sum = 21");
        CHECK(near(sat.rectSum(1, 0, 2, 1), 2 + 3 + 5 + 6), "right 2x2 sub-rect = 16");
        CHECK(near(sat.rectSum(1, 1, 1, 1), 5.0), "single cell (1,1) = 5");
        CHECK(near(sat.rectMean(0, 0, 2, 1), 21.0 / 6.0), "mean of all = 3.5");
    }

    // --- 2. Swapped corners + clamping. ---
    {
        const std::vector<double> g{1, 2, 3, 4, 5, 6};
        SummedAreaTable<double> sat(g, 3, 2);
        CHECK(near(sat.rectSum(2, 1, 1, 0), sat.rectSum(1, 0, 2, 1)), "swapped corners give the same sum");
        CHECK(near(sat.rectSum(-5, -5, 100, 100), 21.0), "out-of-range corners clamp to the whole grid");
    }

    // --- 3. Randomized cross-check. ---
    {
        const int w = 40, h = 30;
        std::vector<double> g(static_cast<std::size_t>(w * h));
        Lcg rng{0xBADF00Du};
        for (double& v : g) v = static_cast<double>(rng.next() % 1000u) - 500.0;
        SummedAreaTable<double> sat(g, w, h);
        bool ok = true;
        for (int t = 0; t < 5000; ++t) {
            int x0 = static_cast<int>(rng.next() % static_cast<std::uint32_t>(w));
            int x1 = static_cast<int>(rng.next() % static_cast<std::uint32_t>(w));
            int y0 = static_cast<int>(rng.next() % static_cast<std::uint32_t>(h));
            int y1 = static_cast<int>(rng.next() % static_cast<std::uint32_t>(h));
            if (x1 < x0) { const int tmp = x0; x0 = x1; x1 = tmp; }
            if (y1 < y0) { const int tmp = y0; y0 = y1; y1 = tmp; }
            if (!near(sat.rectSum(x0, y0, x1, y1), bruteSum(g, w, x0, y0, x1, y1), 1e-6)) ok = false;
        }
        CHECK(ok, "random rectangle sums match brute force");
    }

    // --- 4. Constant-time box blur matches a brute-force blur. ---
    {
        const int w = 24, h = 24, r = 2;
        std::vector<double> g(static_cast<std::size_t>(w * h));
        Lcg rng{0x1234u};
        for (double& v : g) v = static_cast<double>(rng.next() % 256u);
        SummedAreaTable<double> sat(g, w, h);
        bool ok = true;
        for (int y = 0; y < h && ok; ++y) {
            for (int x = 0; x < w; ++x) {
                const int x0 = x - r, x1 = x + r, y0 = y - r, y1 = y + r;
                const double fast = sat.rectMean(x0, y0, x1, y1);
                // Brute-force average over the same clamped window.
                const int cx0 = x0 < 0 ? 0 : x0, cx1 = x1 >= w ? w - 1 : x1;
                const int cy0 = y0 < 0 ? 0 : y0, cy1 = y1 >= h ? h - 1 : y1;
                double s = bruteSum(g, w, cx0, cy0, cx1, cy1);
                const double cnt = static_cast<double>((cx1 - cx0 + 1) * (cy1 - cy0 + 1));
                if (!near(fast, s / cnt)) ok = false;
            }
        }
        CHECK(ok, "SAT box blur equals a brute-force box blur at every pixel");
    }

    // --- 5. Invalid grid. ---
    {
        SummedAreaTable<double> empty(std::vector<double>{}, 0, 0);
        CHECK(!empty.valid() && near(empty.total(), 0.0), "empty grid is invalid, total 0");
        SummedAreaTable<double> mism(std::vector<double>{1, 2, 3}, 2, 2);
        CHECK(!mism.valid(), "size mismatch is rejected");
    }

    if (g_fail == 0) {
        std::printf("summedareatable: OK — known grid, swap/clamp, random sums, box blur, invalid.\n");
        return 0;
    }
    std::printf("summedareatable: %d failure(s).\n", g_fail);
    return 1;
}
