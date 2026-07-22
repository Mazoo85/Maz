// tests/audio/noise.cpp — verifies coloured noise generators (audio::WhiteNoise / PinkNoise / BrownNoise).
// Ground truths: each generator is deterministic (same seed -> same stream) and reset() restores it; every
// sample stays within [-1, 1]; white noise is near-zero-mean, non-constant, and essentially uncorrelated
// sample-to-sample; brown noise is a strongly correlated random walk (high lag-1 autocorrelation); pink noise
// sits between the two — clearly more correlated than white (more low-frequency energy) but less than brown.
// The lag-1 autocorrelation cleanly separates the three colours. Pure CPU, deterministic, statistical checks
// sized so they are not flaky.
#include "maz/audio/Noise.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::audio;

template <typename Gen>
static std::vector<float> gen(Gen g, int n) {
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) out.push_back(g.next());
    return out;
}

static double mean(const std::vector<float>& x) {
    double s = 0;
    for (float v : x) s += v;
    return s / static_cast<double>(x.size());
}

// Lag-1 autocorrelation in [-1, 1]: ~0 = uncorrelated (white), ~1 = a slow random walk (brown).
static double autocorr1(const std::vector<float>& x) {
    const double m = mean(x);
    double num = 0, den = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double d = x[i] - m;
        den += d * d;
        if (i > 0) num += d * (x[i - 1] - m);
    }
    return den > 0 ? num / den : 0.0;
}

int main() {
    const int N = 50000;

    // --- 1. Determinism + reset. ---
    {
        WhiteNoise a(1234), b(1234);
        bool same = true;
        for (int i = 0; i < 1000; ++i)
            if (a.next() != b.next()) same = false;
        CHECK(same, "same seed -> identical white stream");

        PinkNoise p(77);
        const float first = p.next();
        for (int i = 0; i < 500; ++i) p.next();
        p.reset(77);
        CHECK(p.next() == first, "pink reset() restores the sequence");

        WhiteNoise c(1), d(2);
        CHECK(c.next() != d.next(), "different seeds -> different streams");
    }

    // --- 2. Every sample within [-1, 1]. ---
    {
        const auto w = gen(WhiteNoise(5), N);
        const auto p = gen(PinkNoise(5), N);
        const auto br = gen(BrownNoise(5), N);
        bool inRange = true;
        for (int i = 0; i < N; ++i) {
            if (w[static_cast<std::size_t>(i)] < -1.0f || w[static_cast<std::size_t>(i)] >= 1.0001f) inRange = false;
            if (std::fabs(p[static_cast<std::size_t>(i)]) > 1.0f) inRange = false;
            if (std::fabs(br[static_cast<std::size_t>(i)]) > 1.0f) inRange = false;
        }
        CHECK(inRange, "all white/pink/brown samples within [-1, 1]");
    }

    // --- 3. White noise: near-zero mean, non-constant, essentially uncorrelated. ---
    {
        const auto w = gen(WhiteNoise(999), N);
        CHECK(std::fabs(mean(w)) < 0.02, "white noise mean ~ 0");
        float mn = 2, mx = -2;
        for (float v : w) { mn = std::min(mn, v); mx = std::max(mx, v); }
        CHECK(mx - mn > 1.5f, "white noise spans a wide range (not constant)");
        CHECK(std::fabs(autocorr1(w)) < 0.2, "white noise is essentially uncorrelated (|r1| < 0.2)");
    }

    // --- 4. Colours are ordered by lag-1 autocorrelation: white < pink < brown. ---
    {
        const double rW = autocorr1(gen(WhiteNoise(2024), N));
        const double rP = autocorr1(gen(PinkNoise(2024), N));
        const double rB = autocorr1(gen(BrownNoise(2024), N));
        CHECK(rB > 0.9, "brown noise is a strongly correlated random walk (r1 > 0.9)");
        CHECK(rP > rW + 0.15, "pink noise is clearly more correlated than white");
        CHECK(rP < rB, "pink noise is less correlated than brown");
        std::printf("  (autocorr r1: white=%.3f pink=%.3f brown=%.3f)\n", rW, rP, rB);
    }

    if (g_fail == 0) {
        std::printf("noise: OK — deterministic, bounded, white uncorrelated, pink between white and brown.\n");
        return 0;
    }
    std::printf("noise: %d failure(s).\n", g_fail);
    return 1;
}
