// tests/core/p2quantile.cpp — verifies the streaming P-Square quantile estimator (core P2Quantile.hpp).
// Ground truths, deterministic (fixed PCG-style LCG, no <random>, no time):
//   * on a uniform 0..1 stream the p-quantile estimate lands near p (P² converges);
//   * the median of a known ramp is near the true middle;
//   * p95/p99 are ordered above the median and below the max;
//   * fewer than five samples returns an exact interpolated order statistic;
//   * clear() resets the estimator.
#include "maz/core/P2Quantile.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// A tiny deterministic LCG so the test never depends on <random> or the clock.
struct Lcg {
    std::uint64_t s;
    double next01() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 11) / static_cast<double>(1ull << 53);
    }
};

int main() {
    using maz::core::P2Quantile;

    // --- 1. Uniform stream: several quantiles land near their true value. ---
    {
        const double ps[] = {0.25, 0.5, 0.75, 0.95};
        for (double p : ps) {
            P2Quantile est(p);
            Lcg rng{0x1234567u};
            for (int i = 0; i < 100000; ++i) est.push(rng.next01());
            // For U(0,1) the true p-quantile IS p. P² typically within a couple percent.
            CHECK(std::fabs(est.value() - p) < 0.02, "uniform quantile estimate is near p");
        }
    }

    // --- 2. Median of a shuffled 0..N ramp is near the middle. ---
    {
        std::vector<double> data;
        for (int i = 0; i <= 1000; ++i) data.push_back(static_cast<double>(i));
        // Deterministic Fisher-Yates with the LCG.
        Lcg rng{0xC0FFEEu};
        for (std::size_t i = data.size(); i > 1; --i) {
            const std::size_t j = static_cast<std::size_t>(rng.next01() * static_cast<double>(i));
            std::swap(data[i - 1], data[j < i ? j : i - 1]);
        }
        P2Quantile med(0.5);
        for (double x : data) med.push(x);
        CHECK(std::fabs(med.value() - 500.0) < 15.0, "median of a 0..1000 ramp is near 500");
    }

    // --- 3. Ordering: p50 < p95 < p99 <= max on a positive stream. ---
    {
        P2Quantile p50(0.5), p95(0.95), p99(0.99);
        Lcg rng{0xBEEFu};
        double mx = 0.0;
        for (int i = 0; i < 50000; ++i) {
            const double x = rng.next01() * 100.0; // latencies 0..100 ms
            p50.push(x);
            p95.push(x);
            p99.push(x);
            if (x > mx) mx = x;
        }
        CHECK(p50.value() < p95.value(), "p50 below p95");
        CHECK(p95.value() < p99.value(), "p95 below p99");
        CHECK(p99.value() <= mx + 1e-6, "p99 does not exceed the observed max");
        CHECK(p95.value() > 90.0 && p95.value() < 96.0, "p95 of U(0,100) is near 95");
    }

    // --- 4. Fewer than five samples: exact interpolated order statistic. ---
    {
        P2Quantile med(0.5);
        med.push(10.0);
        med.push(30.0);
        med.push(20.0);            // sorted: 10,20,30 -> median 20
        CHECK(std::fabs(med.value() - 20.0) < 1e-9, "median of {10,20,30} is exactly 20");
        P2Quantile one(0.5);
        one.push(7.0);
        CHECK(std::fabs(one.value() - 7.0) < 1e-9, "single sample returns that sample");
    }

    // --- 5. clear() resets. ---
    {
        P2Quantile q(0.5);
        for (int i = 0; i < 100; ++i) q.push(static_cast<double>(i));
        q.clear();
        CHECK(q.count() == 0, "clear resets the count");
        q.push(42.0);
        CHECK(std::fabs(q.value() - 42.0) < 1e-9, "estimator works again after clear");
    }

    if (g_fail == 0) {
        std::printf("p2quantile: OK — uniform convergence, median, p50<p95<p99, small-n exact, clear.\n");
        return 0;
    }
    std::printf("p2quantile: %d failure(s).\n", g_fail);
    return 1;
}
