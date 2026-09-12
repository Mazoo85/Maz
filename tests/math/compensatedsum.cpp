// tests/math/compensatedsum.cpp — verifies compensated (Kahan/Neumaier) summation (math CompensatedSum.hpp).
// Ground truths, deterministic (fixed + seeded-LCG data, no <random>, no clock):
//   * PATHOLOGICAL SUM (the airtight oracle): summing a large value followed by many tiny ones in float32,
//     naive left-to-right addition drops the tiny contributions; the compensated sum matches the EXACT
//     answer (computed independently in double precision) to within a float ULP, while naive drifts far;
//   * NEUMAIER CASE: a large term added AFTER small ones (where plain Kahan degrades) is still exact;
//   * on a big seeded random set, the compensated float32 total is much closer to the double reference than
//     the naive float32 total;
//   * empty sum is 0, a single element is exact, the accumulator resets; determinism.
#include "maz/math/CompensatedSum.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

int main() {
    // --- 1. Big value then many small ones: naive float32 loses them, compensated does not. ---
    {
        std::vector<float> xs;
        xs.push_back(1.0e8f); // at ~1e8 the float32 ULP is 8, so adding 1.0 is lost by naive summation
        const int n = 1000000;
        for (int i = 0; i < n; ++i) xs.push_back(1.0f);
        const double exact = 1.0e8 + static_cast<double>(n); // 101,000,000 exactly
        // Naive float32.
        float naive = 0.0f;
        for (float x : xs) naive += x;
        const float comp = maz::math::compensatedSum(xs);
        CHECK(std::fabs(static_cast<double>(comp) - exact) < 2.0,
              "compensated sum recovers the exact total when small values are swamped by a large one");
        CHECK(std::fabs(static_cast<double>(naive) - exact) > 1000.0,
              "naive float summation drifts far on the same data (confirming the test is meaningful)");
    }

    // --- 2. Neumaier case: small terms first, then a large one. ---
    {
        maz::math::KahanSum<float> acc;
        for (int i = 0; i < 1000; ++i) acc.add(1.0f); // running sum = 1000
        acc.add(1.0e8f);                              // large term added after
        acc.add(-1.0e8f);                             // cancel it
        CHECK(std::fabs(acc.value() - 1000.0f) < 1.0f,
              "Neumaier summation stays exact when a large term is added after small ones");
    }

    // --- 3. On a large random set, compensated float beats naive float vs the double reference. ---
    {
        Lcg rng{0x51ADu};
        std::vector<float> xs;
        double ref = 0.0;
        for (int i = 0; i < 500000; ++i) {
            const float v = (rng.unit() - 0.5f) * 1000.0f + 100000.0f; // large mean, small spread
            xs.push_back(v);
            ref += static_cast<double>(v);
        }
        float naive = 0.0f;
        for (float x : xs) naive += x;
        const float comp = maz::math::compensatedSum(xs);
        const double errNaive = std::fabs(static_cast<double>(naive) - ref);
        const double errComp = std::fabs(static_cast<double>(comp) - ref);
        CHECK(errComp < errNaive * 0.05, "compensated summation is far closer to the true total than naive");
    }

    // --- 4. Edge cases + reset. ---
    {
        const std::vector<float> empty;
        CHECK(maz::math::compensatedSum(empty) == 0.0f, "the sum of an empty range is 0");
        CHECK(maz::math::compensatedSum(std::vector<float>{3.5f}) == 3.5f, "a single element sums to itself");
        maz::math::KahanSum<float> acc;
        acc.add(5.0f);
        acc.add(2.0f);
        acc.reset();
        acc.add(9.0f);
        CHECK(acc.value() == 9.0f, "reset clears the accumulator and its compensation");
    }

    // --- 5. Determinism + double instantiation. ---
    {
        const std::vector<double> xs{1.0, 1e16, -1e16, 2.0, 3.0};
        CHECK(std::fabs(maz::math::compensatedSum(xs) - 6.0) < 1e-6, "double compensated sum is exact");
        const std::vector<float> ys{1, 2, 3, 4, 5};
        CHECK(maz::math::compensatedSum(ys) == maz::math::compensatedSum(ys),
              "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("compensatedsum: OK — pathological recovery, Neumaier case, beats-naive, edges, "
                    "determinism.\n");
        return 0;
    }
    std::printf("compensatedsum: %d failure(s).\n", g_fail);
    return 1;
}
