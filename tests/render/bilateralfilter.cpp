// tests/render/bilateralfilter.cpp — verifies edge-preserving bilateral smoothing (render BilateralFilter.hpp).
// Ground truths, deterministic (seeded LCG for noise, no <random>, no clock):
//   * a constant image is returned unchanged (all weights symmetric, average == the constant);
//   * no overshoot: every output channel lies within the min/max of the input (a convex blend);
//   * the DEFINING property: on a noisy step edge, the bilateral SMOOTHS each flat side (variance drops) yet
//     PRESERVES the edge contrast, unlike a plain blur which would smear it;
//   * determinism.
#include "maz/render/BilateralFilter.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float noise() { return (static_cast<float>(next() % 2001u) / 1000.0f - 1.0f) * 0.03f; } // +-0.03
};

int main() {
    // --- 1. Constant image unchanged. ---
    {
        Image img(24, 24, Color{0.35f, 0.6f, 0.2f, 1.0f});
        const Image f = maz::render::bilateralFilter(img, 2.0f, 0.2f);
        bool ok = true;
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 24; ++x) {
                const Color c = f.getPixel(x, y);
                if (std::fabs(c.r - 0.35f) > 2e-3f || std::fabs(c.g - 0.6f) > 2e-3f || std::fabs(c.b - 0.2f) > 2e-3f)
                    ok = false;
            }
        CHECK(ok, "a constant image is unchanged");
    }

    // --- 2. Noisy step edge: smooth each side, preserve the edge. ---
    {
        const int w = 40, h = 24;
        Image img(w, h);
        Lcg rng{0x5EEDu};
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const float base = x < w / 2 ? 0.15f : 0.85f;
                const float v = base + rng.noise();
                img.setPixel(x, y, Color{v, v, v, 1.0f});
            }
        const Image f = maz::render::bilateralFilter(img, 2.5f, 0.08f); // small range -> respect the edge

        // Variance of the interior of the left (dark) side, before vs after.
        auto sideVar = [&](const Image& im, int x0, int x1) {
            double mean = 0.0; int n = 0;
            for (int y = 4; y < h - 4; ++y) for (int x = x0; x < x1; ++x) { mean += im.getPixel(x, y).r; ++n; }
            mean /= n;
            double var = 0.0;
            for (int y = 4; y < h - 4; ++y) for (int x = x0; x < x1; ++x) { const double d = im.getPixel(x, y).r - mean; var += d * d; }
            return var / n;
        };
        const double vBefore = sideVar(img, 4, w / 2 - 4);
        const double vAfter = sideVar(f, 4, w / 2 - 4);
        CHECK(vAfter < vBefore * 0.5, "flat-region noise is smoothed (variance drops)");

        // Edge preserved: mean of the far-left stays dark, far-right stays bright -> contrast ~0.7.
        double leftMean = 0.0, rightMean = 0.0; int nl = 0, nr = 0;
        for (int y = 4; y < h - 4; ++y) {
            for (int x = 2; x < 8; ++x) { leftMean += f.getPixel(x, y).r; ++nl; }
            for (int x = w - 8; x < w - 2; ++x) { rightMean += f.getPixel(x, y).r; ++nr; }
        }
        leftMean /= nl; rightMean /= nr;
        CHECK(leftMean < 0.25 && rightMean > 0.75, "each side keeps its own tone (edge not smeared across)");
        CHECK(rightMean - leftMean > 0.55, "the edge contrast is preserved");
    }

    // --- 3. No overshoot (convex blend stays within the neighbourhood range). ---
    {
        const int w = 20, h = 20;
        Image img(w, h);
        Lcg rng{0x1357u};
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const float v = 0.5f + rng.noise() * 5.0f; // +-0.15
                img.setPixel(x, y, Color{v, v * 0.8f, 0.3f, 1.0f});
            }
        const Image f = maz::render::bilateralFilter(img, 2.0f, 0.2f);
        bool bounded = true;
        for (int y = 1; y < h - 1; ++y)
            for (int x = 1; x < w - 1; ++x) {
                float mn = 1e9f, mx = -1e9f;
                for (int dy = -3; dy <= 3; ++dy)
                    for (int dx = -3; dx <= 3; ++dx) {
                        const int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        const float r = img.getPixel(nx, ny).r;
                        mn = std::fmin(mn, r); mx = std::fmax(mx, r);
                    }
                const float r = f.getPixel(x, y).r;
                if (r < mn - 1e-3f || r > mx + 1e-3f) bounded = false;
            }
        CHECK(bounded, "output never overshoots the input neighbourhood (convex blend)");
    }

    // --- 4. Determinism. ---
    {
        Image img(16, 16, Color{0.4f, 0.4f, 0.4f, 1.0f});
        img.setPixel(8, 8, Color{0.9f, 0.1f, 0.1f, 1.0f});
        const Image a = maz::render::bilateralFilter(img, 2.0f, 0.15f);
        const Image b = maz::render::bilateralFilter(img, 2.0f, 0.15f);
        CHECK(a.data() == b.data(), "identical inputs produce identical output");
    }

    if (g_fail == 0) {
        std::printf("bilateralfilter: OK — constant unchanged, noise smoothed, edge preserved, no overshoot, determinism.\n");
        return 0;
    }
    std::printf("bilateralfilter: %d failure(s).\n", g_fail);
    return 1;
}
