// tests/audio/karplusstrong.cpp — verifies Karplus-Strong plucked-string synthesis (audio KarplusStrong.hpp).
// Ground truths, deterministic (seeded noise burst, no <random>, no clock):
//   * PITCH: the autocorrelation of the output peaks at a lag equal to the delay length (sampleRate/freq),
//     over a range of frequencies — i.e. the tone really is at the requested pitch;
//   * an octave up halves the period;
//   * DECAY: a late window carries less energy than an early one (the string rings down);
//   * output stays bounded in ~[-1,1]; determinism; degenerate inputs return empty.
#include "maz/audio/KarplusStrong.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Estimate the fundamental period (in samples) by autocorrelation over a plausible lag range.
static int estimatePeriod(const std::vector<float>& x, int minLag, int maxLag) {
    int best = minLag;
    double bestScore = -1e30;
    const int win = 4000;
    const int n = static_cast<int>(x.size());
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double acc = 0.0;
        for (int i = 0; i < win && i + lag < n; ++i) acc += static_cast<double>(x[static_cast<std::size_t>(i)]) *
                                                          static_cast<double>(x[static_cast<std::size_t>(i + lag)]);
        if (acc > bestScore) {
            bestScore = acc;
            best = lag;
        }
    }
    return best;
}

static double energy(const std::vector<float>& x, int from, int count) {
    double e = 0.0;
    for (int i = from; i < from + count && i < static_cast<int>(x.size()); ++i)
        e += static_cast<double>(x[static_cast<std::size_t>(i)]) * static_cast<double>(x[static_cast<std::size_t>(i)]);
    return e;
}

int main() {
    const int sr = 44100;

    // --- 1. Pitch matches the delay length across frequencies. ---
    {
        bool ok = true;
        for (float freq : {220.0f, 330.0f, 440.0f, 660.0f}) {
            const int expected = static_cast<int>(std::lround(static_cast<double>(sr) / static_cast<double>(freq)));
            const std::vector<float> x = maz::audio::karplusStrongPluck(freq, sr, 20000, 0.996f, 12345u);
            const int period = estimatePeriod(x, expected - 8, expected + 8);
            if (std::abs(period - expected) > 1) ok = false;
        }
        CHECK(ok, "autocorrelation period matches sampleRate/frequency (correct pitch)");
    }

    // --- 2. An octave up halves the period. ---
    {
        const std::vector<float> lo = maz::audio::karplusStrongPluck(220.0f, sr, 20000, 0.996f, 7u);
        const std::vector<float> hi = maz::audio::karplusStrongPluck(440.0f, sr, 20000, 0.996f, 7u);
        const int pLo = estimatePeriod(lo, 190, 210);
        const int pHi = estimatePeriod(hi, 90, 110);
        CHECK(std::abs(pLo - 2 * pHi) <= 2, "an octave up halves the fundamental period");
    }

    // --- 3. The string decays: late energy < early energy. ---
    {
        const std::vector<float> x = maz::audio::karplusStrongPluck(330.0f, sr, 40000, 0.99f, 99u);
        const double early = energy(x, 0, 4000);
        const double late = energy(x, 30000, 4000);
        CHECK(late < early * 0.6, "energy decays over time (the note rings down)");
    }

    // --- 4. Bounded output + determinism + degenerate. ---
    {
        const std::vector<float> x = maz::audio::karplusStrongPluck(440.0f, sr, 8000, 0.997f, 3u);
        bool bounded = true;
        for (float v : x) if (v < -1.05f || v > 1.05f) bounded = false;
        CHECK(bounded, "output stays within ~[-1,1]");

        const std::vector<float> y = maz::audio::karplusStrongPluck(440.0f, sr, 8000, 0.997f, 3u);
        CHECK(x == y, "same parameters + seed -> identical output");

        CHECK(maz::audio::karplusStrongPluck(0.0f, sr, 100, 0.99f, 1u).empty(), "zero frequency -> empty");
        CHECK(maz::audio::karplusStrongPluck(440.0f, sr, 0, 0.99f, 1u).empty(), "zero samples -> empty");
    }

    if (g_fail == 0) {
        std::printf("karplusstrong: OK — pitch/period, octave, decay, bounded, determinism, degenerate.\n");
        return 0;
    }
    std::printf("karplusstrong: %d failure(s).\n", g_fail);
    return 1;
}
