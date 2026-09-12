// tests/core/randomdistributions.cpp — verifies Poisson/exponential/geometric sampling
// (core RandomDistributions.hpp). Ground truths, deterministic (a fixed LCG uniform source, no <random>):
//   * exponential(rate): all samples > 0; sample mean ~ 1/rate; sample variance ~ 1/rate^2;
//   * poisson(lambda): all samples >= 0; sample mean ~ lambda; sample variance ~ lambda;
//     the large-lambda (normal-approx) branch also has mean ~ lambda;
//   * geometric(p): all samples >= 1; sample mean ~ 1/p;
//   * degenerate params (rate<=0, lambda<=0, p>=1, p<=0) return the documented edge values.
#include "maz/core/RandomDistributions.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// A tiny deterministic LCG returning doubles in [0,1) — no <random>, no clock.
struct Lcg {
    std::uint64_t s;
    double operator()() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 11) / static_cast<double>(1ull << 53);
    }
};

int main() {
    using maz::core::exponential;
    using maz::core::geometric;
    using maz::core::poisson;

    const int N = 400000;

    // --- 1. Exponential: positivity, mean, variance. ---
    {
        Lcg u{0xABCDEFu};
        const double rate = 2.5;         // mean 0.4, var 0.16
        double sum = 0.0, sumsq = 0.0;
        bool allpos = true;
        for (int i = 0; i < N; ++i) {
            const double x = exponential(u, rate);
            if (x <= 0.0) allpos = false;
            sum += x;
            sumsq += x * x;
        }
        const double mean = sum / N;
        const double var = sumsq / N - mean * mean;
        CHECK(allpos, "exponential samples are strictly positive");
        CHECK(std::fabs(mean - 1.0 / rate) < 0.01, "exponential mean ~ 1/rate");
        CHECK(std::fabs(var - 1.0 / (rate * rate)) < 0.02, "exponential variance ~ 1/rate^2");
    }

    // --- 2. Poisson (Knuth branch): non-negativity, mean, variance. ---
    {
        Lcg u{0x13579u};
        const double lambda = 4.0;       // mean 4, var 4
        double sum = 0.0, sumsq = 0.0;
        bool nonneg = true;
        for (int i = 0; i < N; ++i) {
            const int k = poisson(u, lambda);
            if (k < 0) nonneg = false;
            sum += k;
            sumsq += static_cast<double>(k) * k;
        }
        const double mean = sum / N;
        const double var = sumsq / N - mean * mean;
        CHECK(nonneg, "poisson samples are non-negative");
        CHECK(std::fabs(mean - lambda) < 0.05, "poisson mean ~ lambda");
        CHECK(std::fabs(var - lambda) < 0.15, "poisson variance ~ lambda");
    }

    // --- 3. Poisson large-lambda (normal-approx branch): mean. ---
    {
        Lcg u{0x2468u};
        const double lambda = 800.0;     // above the 500 threshold -> normal-approx path
        double sum = 0.0;
        bool nonneg = true;
        const int M = 60000;
        for (int i = 0; i < M; ++i) {
            const int k = poisson(u, lambda);
            if (k < 0) nonneg = false;
            sum += k;
        }
        const double mean = sum / M;
        CHECK(nonneg, "large-lambda poisson samples are non-negative");
        CHECK(std::fabs(mean - lambda) < 2.0, "large-lambda poisson mean ~ lambda");
    }

    // --- 4. Geometric: >= 1 and mean ~ 1/p. ---
    {
        Lcg u{0x99AA55u};
        const double p = 0.2;            // mean 5
        double sum = 0.0;
        bool atleast1 = true;
        for (int i = 0; i < N; ++i) {
            const int t = geometric(u, p);
            if (t < 1) atleast1 = false;
            sum += t;
        }
        const double mean = sum / N;
        CHECK(atleast1, "geometric samples are at least 1");
        CHECK(std::fabs(mean - 1.0 / p) < 0.05, "geometric mean ~ 1/p");
    }

    // --- 5. Degenerate parameters. ---
    {
        Lcg u{0x1u};
        CHECK(exponential(u, 0.0) == 0.0, "exponential with rate<=0 returns 0");
        CHECK(poisson(u, 0.0) == 0, "poisson with lambda<=0 returns 0");
        CHECK(geometric(u, 1.0) == 1, "geometric with p>=1 returns 1");
        CHECK(geometric(u, 0.0) == 0, "geometric with p<=0 returns 0");
    }

    if (g_fail == 0) {
        std::printf("randomdistributions: OK — exponential/poisson/geometric moments + edge cases.\n");
        return 0;
    }
    std::printf("randomdistributions: %d failure(s).\n", g_fail);
    return 1;
}
