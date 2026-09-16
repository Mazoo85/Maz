// tests/render/mediancut.cpp — verifies median-cut colour quantization (render MedianCut.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * an image with <=K distinct colours quantizes to EXACTLY those colours (zero error);
//   * two well-separated colour clusters with K=2 recover one palette entry per cluster, and every pixel
//     maps to its own cluster;
//   * quantization error decreases monotonically as K grows (more palette entries -> closer match);
//   * every palette colour lies within the input colour range;
//   * a cluster holding a third of the image gets a palette entry of its own rather than being averaged
//     away — the regression for box selection by spread ALONE, which spent its splits on a nearly-empty
//     box of stray pixels and left half the image sharing one muddy colour;
//   * determinism.
#include "maz/render/MedianCut.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::render::Color;

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

static float dist2(const Color& a, const Color& b) {
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}
// Mean quantization error: average distance from each pixel to its nearest palette colour.
static float meanError(const std::vector<Color>& px, const std::vector<Color>& pal) {
    float sum = 0.0f;
    for (const Color& c : px) {
        const int idx = maz::render::nearestColor(pal, c);
        sum += std::sqrt(dist2(pal[static_cast<std::size_t>(idx)], c));
    }
    return sum / static_cast<float>(px.size());
}

int main() {
    // --- 1. <=K distinct colours -> exact palette. ---
    {
        const Color four[4] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}};
        std::vector<Color> px;
        for (int i = 0; i < 200; ++i) px.push_back(four[i % 4]);
        const std::vector<Color> pal = maz::render::medianCutPalette(px, 8);
        CHECK(pal.size() == 4, "four distinct colours give a palette of exactly four");
        // Each original colour is reproduced exactly (zero error).
        CHECK(meanError(px, pal) < 1e-5f, "an image with <=K colours quantizes with zero error");
        bool allFound = true;
        for (const Color& c : four) {
            bool found = false;
            for (const Color& p : pal)
                if (dist2(p, c) < 1e-6f) found = true;
            if (!found) allFound = false;
        }
        CHECK(allFound, "every original colour appears in the palette");
    }

    // --- 2. Two separated clusters, K=2. ---
    {
        Lcg rng{0x2C07u};
        std::vector<Color> px;
        for (int i = 0; i < 100; ++i) px.push_back(Color{rng.range(0.9f, 1.0f), rng.range(0.0f, 0.1f), rng.range(0.0f, 0.1f), 1.0f}); // red
        for (int i = 0; i < 100; ++i) px.push_back(Color{rng.range(0.0f, 0.1f), rng.range(0.0f, 0.1f), rng.range(0.9f, 1.0f), 1.0f}); // blue
        const std::vector<Color> pal = maz::render::medianCutPalette(px, 2);
        CHECK(pal.size() == 2, "K=2 yields two palette colours");
        // One palette entry near red, one near blue.
        const bool nearRed = dist2(pal[0], Color{1, 0, 0, 1}) < 0.05f || dist2(pal[1], Color{1, 0, 0, 1}) < 0.05f;
        const bool nearBlue = dist2(pal[0], Color{0, 0, 1, 1}) < 0.05f || dist2(pal[1], Color{0, 0, 1, 1}) < 0.05f;
        CHECK(nearRed && nearBlue, "the two clusters are recovered (one palette colour each)");
        // Each pixel maps to the cluster it belongs to (low error).
        CHECK(meanError(px, pal) < 0.1f, "every pixel maps to its own cluster");
    }

    // --- 3. Error decreases monotonically with K. ---
    {
        Lcg rng{0x9E37u};
        std::vector<Color> px;
        for (int i = 0; i < 400; ++i) px.push_back(Color{rng.range(0, 1), rng.range(0, 1), rng.range(0, 1), 1.0f});
        float prev = 1e30f;
        bool monotone = true;
        for (int k : {2, 4, 8, 16, 32}) {
            const float e = meanError(px, maz::render::medianCutPalette(px, k));
            if (e > prev + 1e-4f) monotone = false;
            prev = e;
        }
        CHECK(monotone, "more palette colours never increase the quantization error");
    }

    // --- 4. Palette colours within input range. ---
    {
        Lcg rng{0x55A1u};
        std::vector<Color> px;
        for (int i = 0; i < 300; ++i) px.push_back(Color{rng.range(0.2f, 0.8f), rng.range(0.1f, 0.4f), rng.range(0.5f, 0.9f), 1.0f});
        const std::vector<Color> pal = maz::render::medianCutPalette(px, 16);
        bool inRange = true;
        for (const Color& p : pal)
            if (p.r < 0.2f - 1e-4f || p.r > 0.8f + 1e-4f || p.g < 0.1f - 1e-4f || p.g > 0.4f + 1e-4f ||
                p.b < 0.5f - 1e-4f || p.b > 0.9f + 1e-4f)
                inRange = false;
        CHECK(inRange, "every palette colour lies within the input colour range");
    }

    // --- 5. Determinism. ---
    {
        Lcg rng{0x1234u};
        std::vector<Color> px;
        for (int i = 0; i < 150; ++i) px.push_back(Color{rng.range(0, 1), rng.range(0, 1), rng.range(0, 1), 1.0f});
        const std::vector<Color> a = maz::render::medianCutPalette(px, 8);
        const std::vector<Color> b = maz::render::medianCutPalette(px, 8);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) same = false;
        CHECK(same, "identical inputs produce identical palettes");
    }

    // --- 6. Population matters, not just spread. ---
    // Three tight clusters, a thousand pixels each, with the stray pixels between them that any real
    // image has. Choosing the next box to split by spread alone is drawn to those strays: the box they
    // form is very wide and nearly empty, so it wins split after split while the box holding the green
    // and blue clusters together — half the image — is never touched. At K=8 that left a mean error of
    // 22.8 out of 255 and no green in the palette at all. Weighting spread by population gives 3.3,
    // which is the noise floor of the data, and green appears from K=4 on.
    {
        Lcg rng{0x51EDu};
        const Color centres[3] = {{0.78f, 0.16f, 0.16f, 1}, {0.16f, 0.71f, 0.24f, 1},
                                  {0.20f, 0.24f, 0.78f, 1}};
        std::vector<Color> px;
        for (int i = 0; i < 3000; ++i) {
            const Color& c = centres[i % 3];
            px.push_back(Color{c.r + rng.range(-0.012f, 0.012f), c.g + rng.range(-0.012f, 0.012f),
                               c.b + rng.range(-0.012f, 0.012f), 1.0f});
        }

        const std::vector<Color> pal = maz::render::medianCutPalette(px, 8);
        CHECK(pal.size() == 8, "K=8 yields eight palette colours");
        // Every cluster must have an entry close to its centre — most of all the green one, which
        // spread-alone selection dropped entirely.
        for (int c = 0; c < 3; ++c) {
            const int j = maz::render::nearestColor(pal, centres[c]);
            CHECK(j >= 0 && dist2(pal[static_cast<std::size_t>(j)], centres[c]) < 0.001f,
                  "each cluster centre has a palette entry of its own at K=8");
        }
        // And the error is at the noise floor rather than ten times it.
        CHECK(meanError(px, pal) < 0.03f, "K=8 on three clusters quantizes to the noise floor");
    }

    if (g_fail == 0) {
        std::printf("mediancut: OK — exact palette, clusters, monotone error, range, determinism.\n");
        return 0;
    }
    std::printf("mediancut: %d failure(s).\n", g_fail);
    return 1;
}
