// Unit tests for maz::core::Rng (PCG32). Deterministic PRNG: exact-equality
// checks on the canonical output vectors, plus loose statistical bounds on the
// derived helpers. Pure C++, no GPU required.

#include "maz/core/Random.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

} // namespace

int main() {
    using core::Rng;

    // --- CANONICAL VECTORS (KEY gate) ----------------------------------------
    // Reference PCG32 output for seed=42, seq=54. If these fail, the seeding or
    // algorithm is wrong.
    {
        Rng r(42, 54);
        check(r.nextU32() == 0xA15C02B7u, "canonical vector 0");
        check(r.nextU32() == 0x7B47F409u, "canonical vector 1");
        check(r.nextU32() == 0xBA1D3330u, "canonical vector 2");
    }

    // --- DETERMINISM ---------------------------------------------------------
    {
        Rng a(42, 54);
        Rng b(42, 54);
        bool identical = true;
        for (int i = 0; i < 100; ++i) {
            if (a.nextU32() != b.nextU32()) {
                identical = false;
            }
        }
        check(identical, "same seed produces identical sequence (100 draws)");
    }

    // --- DIFFERENT SEEDS -----------------------------------------------------
    {
        Rng a(1, 54);
        Rng b(2, 54);
        bool anyDifferent = false;
        for (int i = 0; i < 8; ++i) {
            if (a.nextU32() != b.nextU32()) {
                anyDifferent = true;
            }
        }
        check(anyDifferent, "different seeds produce different sequences");
    }

    // --- nextFloat -----------------------------------------------------------
    {
        Rng r(12345, 54);
        const int N = 100000;
        bool allInRange = true;
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float v = r.nextFloat();
            if (!(v >= 0.0f && v < 1.0f)) {
                allInRange = false;
            }
            sum += static_cast<double>(v);
        }
        check(allInRange, "nextFloat stays in [0,1)");
        check(approx(static_cast<float>(sum / N), 0.5f, 0.01f), "nextFloat mean ~= 0.5");
    }

    // --- rangeInt ------------------------------------------------------------
    {
        Rng r(777, 54);
        const int N = 10000;
        bool allInRange = true;
        bool sawLo = false;
        bool sawHi = false;
        for (int i = 0; i < N; ++i) {
            int v = r.rangeInt(-5, 10);
            if (v < -5 || v > 10) {
                allInRange = false;
            }
            if (v == -5) {
                sawLo = true;
            }
            if (v == 10) {
                sawHi = true;
            }
        }
        check(allInRange, "rangeInt stays within [-5,10]");
        check(sawLo, "rangeInt reaches low bound (-5)");
        check(sawHi, "rangeInt reaches high bound (10)");

        Rng r2(999, 54);
        check(r2.rangeInt(7, 7) == 7, "rangeInt(k,k) == k");

        // Wide span (> 2^31): exercises the 64-bit narrowing path — a plain
        // `lo + (int)(r % range)` would overflow int (UB) here.
        Rng r3(31337, 54);
        bool wideInRange = true;
        for (int i = 0; i < 10000; ++i) {
            int v = r3.rangeInt(-1, INT_MAX);
            if (v < -1) {
                wideInRange = false;
            }
        }
        check(wideInRange, "rangeInt wide span (-1..INT_MAX) stays in bounds");
    }

    // --- nextBool ------------------------------------------------------------
    {
        Rng r(2024, 54);
        const int N = 200;
        bool sawTrue = false;
        bool sawFalse = false;
        for (int i = 0; i < N; ++i) {
            if (r.nextBool()) {
                sawTrue = true;
            } else {
                sawFalse = true;
            }
        }
        check(sawTrue, "nextBool produces true");
        check(sawFalse, "nextBool produces false");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
