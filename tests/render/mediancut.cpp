// tests/render/mediancut.cpp — verifies median-cut colour quantization (render MedianCut.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * an image with <=K distinct colours quantizes to EXACTLY those colours (zero error);
//   * two well-separated colour clusters with K=2 recover one palette entry per cluster, and every pixel
//     maps to its own cluster;
//   * quantization error decreases monotonically as K grows (more palette entries -> closer match);
//   * every palette colour lies within the input colour range;
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

    if (g_fail == 0) {
        std::printf("mediancut: OK — exact palette, clusters, monotone error, range, determinism.\n");
        return 0;
    }
    std::printf("mediancut: %d failure(s).\n", g_fail);
    return 1;
}
