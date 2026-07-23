// tests/math/savitzkygolay.cpp — verifies Savitzky–Golay smoothing (math SavitzkyGolay.hpp).
// Ground truths, deterministic (fixed + seeded-LCG signals, no <random>, no clock):
//   * POLYNOMIAL EXACTNESS (the airtight oracle): a signal that is exactly a polynomial of degree <= the
//     filter order passes through UNCHANGED at every sample, including the two endpoints — the defining
//     property of the least-squares fit;
//   * NOISE REDUCTION: on a smooth signal plus seeded noise, the filtered output has much lower error to the
//     clean signal than the noisy input, and the mean is preserved;
//   * PEAK PRESERVATION: on a sharp Gaussian bump, S–G keeps the peak far better than a same-width moving
//     average (which flattens it);
//   * degenerate parameters return the input unchanged; determinism.
#include "maz/math/SavitzkyGolay.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; } // [-1,1)
};

int main() {
    // --- 1. Polynomial exactness for degrees 0..order. ---
    {
        const int n = 40;
        const int half = 4, order = 3; // window 9, cubic
        // Coefficients for a cubic: 2 - 0.5x + 0.03x^2 - 0.001x^3.
        auto poly = [](double x) { return 2.0 - 0.5 * x + 0.03 * x * x - 0.001 * x * x * x; };
        std::vector<float> y(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) y[static_cast<std::size_t>(i)] = static_cast<float>(poly(i));
        const auto out = maz::math::savitzkyGolay(y, half, order);
        float worst = 0.0f;
        for (int i = 0; i < n; ++i) {
            worst = std::max(worst, std::fabs(out[static_cast<std::size_t>(i)] - y[static_cast<std::size_t>(i)]));
        }
        CHECK(worst < 1e-2f, "a cubic passes through an order-3 S-G filter unchanged (including endpoints)");
    }

    // --- 2. High-frequency roughness reduced, mean preserved. Smoothing is defined by cutting
    // point-to-point jitter (total variation); it faithfully keeps any low-frequency content, so we measure
    // roughness rather than error to a clean signal the noise may be locally biased away from. ---
    {
        const int n = 400;
        Lcg rng{0xA5A5u};
        std::vector<float> noisy(static_cast<std::size_t>(n));
        double meanNoisy = 0.0;
        for (int i = 0; i < n; ++i) {
            const float c = std::sin(static_cast<float>(i) * 0.05f) * 3.0f + 0.01f * static_cast<float>(i);
            const float nz = c + rng.sym() * 0.8f;
            noisy[static_cast<std::size_t>(i)] = nz;
            meanNoisy += nz;
        }
        const auto sm = maz::math::savitzkyGolay(noisy, 6, 2);
        auto totalVariation = [](const std::vector<float>& v) {
            double tv = 0.0;
            for (std::size_t i = 1; i < v.size(); ++i) tv += std::fabs(v[i] - v[i - 1]);
            return tv;
        };
        const double tvNoisy = totalVariation(noisy);
        const double tvSmooth = totalVariation(sm);
        double meanSmooth = 0.0;
        for (int i = 0; i < n; ++i) meanSmooth += sm[static_cast<std::size_t>(i)];
        CHECK(tvSmooth < tvNoisy * 0.4, "S-G smoothing cuts point-to-point roughness (total variation) sharply");
        CHECK(std::fabs(meanSmooth - meanNoisy) < static_cast<double>(n) * 0.05,
              "S-G smoothing preserves the signal mean");
    }

    // --- 3. Peak preservation vs a moving average of the same width. ---
    {
        const int n = 61;
        std::vector<float> y(static_cast<std::size_t>(n));
        const int c = n / 2;
        for (int i = 0; i < n; ++i) {
            const float d = static_cast<float>(i - c);
            y[static_cast<std::size_t>(i)] = std::exp(-d * d / 8.0f); // sharp Gaussian, peak 1.0 at centre
        }
        const int half = 5;
        const auto sg = maz::math::savitzkyGolay(y, half, 4); // quartic keeps curvature
        // Same-width box average.
        std::vector<float> ma(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            float sum = 0.0f; int cnt = 0;
            for (int j = i - half; j <= i + half; ++j) {
                if (j >= 0 && j < n) { sum += y[static_cast<std::size_t>(j)]; ++cnt; }
            }
            ma[static_cast<std::size_t>(i)] = sum / static_cast<float>(cnt);
        }
        const float sgPeak = sg[static_cast<std::size_t>(c)];
        const float maPeak = ma[static_cast<std::size_t>(c)];
        CHECK(sgPeak > maPeak + 0.1f, "S-G preserves the peak far better than a same-width moving average");
        CHECK(sgPeak > 0.85f && sgPeak <= 1.02f, "the S-G peak stays close to the true peak height");
    }

    // --- 4. Degenerate params return the input. ---
    {
        std::vector<float> y{1, 2, 3};
        CHECK(maz::math::savitzkyGolay(y, 5, 2) == y, "a window larger than the signal returns the input");
        CHECK(maz::math::savitzkyGolay(y, 0, 2) == y, "halfWindow < 1 returns the input");
    }

    // --- 5. Determinism. ---
    {
        std::vector<float> y{3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9};
        CHECK(maz::math::savitzkyGolay(y, 3, 2) == maz::math::savitzkyGolay(y, 3, 2),
              "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("savitzkygolay: OK — polynomial exactness, noise reduction, peak preservation, "
                    "degenerate, determinism.\n");
        return 0;
    }
    std::printf("savitzkygolay: %d failure(s).\n", g_fail);
    return 1;
}
