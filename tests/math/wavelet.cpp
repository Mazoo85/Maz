// tests/math/wavelet.cpp — verifies the normalised Haar wavelet transform (math Wavelet.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * PERFECT RECONSTRUCTION: inverse(forward(x)) == x for 1D and 2D (airtight round-trip);
//   * ENERGY PRESERVATION (Parseval): the orthonormal Haar basis keeps sum-of-squares identical;
//   * a constant image transforms to a single non-zero DC coefficient, everything else zero;
//   * COMPRESSION: zeroing the smallest detail coefficients reconstructs with bounded, monotonically
//     increasing error as more coefficients are dropped;
//   * determinism.
#include "maz/math/Wavelet.hpp"

#include <algorithm>
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

static float energy(const std::vector<float>& v) {
    float e = 0.0f;
    for (float x : v) e += x * x;
    return e;
}
static float maxAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float m = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

int main() {
    Lcg rng{0x3AA7u};

    // --- 1. 1D perfect reconstruction + energy preservation. ---
    {
        std::vector<float> x(16);
        for (float& v : x) v = rng.range(-10.0f, 10.0f);
        const std::vector<float> c = maz::math::haarForward1D(x);
        const std::vector<float> r = maz::math::haarInverse1D(c);
        CHECK(maxAbsDiff(x, r) < 1e-4f, "1D inverse(forward(x)) reconstructs the signal");
        CHECK(std::fabs(energy(x) - energy(c)) < 1e-3f, "1D Haar preserves energy (orthonormal)");
    }

    // --- 2. 2D perfect reconstruction + energy preservation. ---
    {
        for (std::size_t n : {(std::size_t)8, (std::size_t)16}) {
            std::vector<float> img(n * n);
            for (float& v : img) v = rng.range(0.0f, 1.0f);
            const std::vector<float> c = maz::math::haarForward2D(img, n);
            const std::vector<float> r = maz::math::haarInverse2D(c, n);
            CHECK(maxAbsDiff(img, r) < 1e-4f, "2D inverse(forward(img)) reconstructs the image");
            CHECK(std::fabs(energy(img) - energy(c)) < 1e-2f, "2D Haar preserves energy (orthonormal)");
        }
    }

    // --- 3. Constant image -> single DC coefficient. ---
    {
        const std::size_t n = 8;
        std::vector<float> img(n * n, 0.5f);
        const std::vector<float> c = maz::math::haarForward2D(img, n);
        CHECK(std::fabs(c[0]) > 1e-3f, "constant image has a non-zero DC coefficient");
        bool restZero = true;
        for (std::size_t i = 1; i < c.size(); ++i)
            if (std::fabs(c[i]) > 1e-4f) restZero = false;
        CHECK(restZero, "a constant image has zero detail coefficients");
        // DC squared should carry all the energy.
        CHECK(std::fabs(c[0] * c[0] - energy(img)) < 1e-2f, "the DC coefficient holds all the energy");
    }

    // --- 4. Compression: dropping small coefficients gives bounded, monotone error. ---
    {
        const std::size_t n = 16;
        // A smooth-ish image (mostly low-frequency) so most detail is small.
        std::vector<float> img(n * n);
        for (std::size_t y = 0; y < n; ++y)
            for (std::size_t x = 0; x < n; ++x)
                img[y * n + x] = std::sin(static_cast<float>(x) * 0.4f) * std::cos(static_cast<float>(y) * 0.3f);
        const std::vector<float> c = maz::math::haarForward2D(img, n);

        auto compressError = [&](float dropFraction) {
            std::vector<float> mags;
            for (float v : c) mags.push_back(std::fabs(v));
            std::sort(mags.begin(), mags.end());
            const std::size_t idx = static_cast<std::size_t>(dropFraction * static_cast<float>(mags.size()));
            const float thresh = mags[std::min(idx, mags.size() - 1)];
            std::vector<float> cc = c;
            for (float& v : cc)
                if (std::fabs(v) < thresh) v = 0.0f;
            const std::vector<float> r = maz::math::haarInverse2D(cc, n);
            return maxAbsDiff(img, r);
        };
        const float e50 = compressError(0.5f);
        const float e90 = compressError(0.9f);
        CHECK(e50 < 0.15f, "dropping the smallest 50% of coefficients barely changes the image");
        CHECK(e90 >= e50 - 1e-6f, "dropping more coefficients never reduces error (monotone)");
    }

    // --- 5. Determinism. ---
    {
        std::vector<float> x(32);
        for (float& v : x) v = rng.range(-5.0f, 5.0f);
        const std::vector<float> a = maz::math::haarForward1D(x);
        const std::vector<float> b = maz::math::haarForward1D(x);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i] != b[i]) same = false;
        CHECK(same, "identical inputs produce identical transforms");
    }

    if (g_fail == 0) {
        std::printf("wavelet: OK — reconstruction, energy, DC, compression, determinism.\n");
        return 0;
    }
    std::printf("wavelet: %d failure(s).\n", g_fail);
    return 1;
}
