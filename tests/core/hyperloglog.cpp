// tests/core/hyperloglog.cpp — verifies the HyperLogLog cardinality estimator (core HyperLogLog.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * for streams of N distinct items (N = 100..200000) the estimate is within a few percent of N;
//   * adding the SAME item many times does not inflate the count (distinct, not total);
//   * an empty sketch estimates ~0;
//   * merging two sketches over disjoint halves estimates the whole set's cardinality;
//   * estimation is deterministic.
#include "maz/core/HyperLogLog.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using maz::core::HyperLogLog;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    // --- 1. Accuracy across magnitudes. ---
    {
        const int Ns[] = {100, 1000, 10000, 50000, 200000};
        bool ok = true;
        for (int N : Ns) {
            HyperLogLog hll(12); // m = 4096, ~1.6% standard error
            for (int i = 0; i < N; ++i) hll.add("item_" + std::to_string(i));
            const double est = hll.estimate();
            const double rel = std::fabs(est - static_cast<double>(N)) / static_cast<double>(N);
            if (rel > 0.05) { // 5% — a few standard errors of slack
                std::printf("  N=%d est=%.0f rel=%.3f\n", N, est, rel);
                ok = false;
            }
        }
        CHECK(ok, "distinct-count estimate is within 5% across magnitudes");
    }

    // --- 2. Duplicates don't inflate the count. ---
    {
        HyperLogLog hll(12);
        for (int rep = 0; rep < 1000; ++rep)
            for (int i = 0; i < 500; ++i) hll.add("dup_" + std::to_string(i));
        const double est = hll.estimate();
        CHECK(std::fabs(est - 500.0) < 500.0 * 0.1, "500 distinct items added 1000x each still estimate ~500");
    }

    // --- 3. Empty sketch. ---
    {
        HyperLogLog hll(12);
        CHECK(hll.estimate() < 1.0, "empty sketch estimates ~0");
    }

    // --- 4. Merge estimates the union. ---
    {
        HyperLogLog a(12), b(12);
        for (int i = 0; i < 30000; ++i) a.add("k_" + std::to_string(i));
        for (int i = 30000; i < 60000; ++i) b.add("k_" + std::to_string(i));
        a.merge(b);
        const double est = a.estimate();
        CHECK(std::fabs(est - 60000.0) / 60000.0 < 0.05, "merged sketch estimates the union cardinality");
    }

    // --- 5. Determinism. ---
    {
        HyperLogLog a(10), b(10);
        for (int i = 0; i < 5000; ++i) {
            const std::string s = "d_" + std::to_string(i);
            a.add(s);
            b.add(s);
        }
        CHECK(a.estimate() == b.estimate(), "same items -> identical estimate");
    }

    if (g_fail == 0) {
        std::printf("hyperloglog: OK — accuracy across magnitudes, dedup, empty, merge, determinism.\n");
        return 0;
    }
    std::printf("hyperloglog: %d failure(s).\n", g_fail);
    return 1;
}
