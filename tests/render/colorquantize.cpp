// tests/render/colorquantize.cpp — verifies render::quantizePalette / mapToPalette /
// nearestPaletteIndex, the 0..255 integer median-cut palette builder.
//
// tests/unit/main.cpp already covers the degenerate cases and the budget. What it could not catch,
// because it only asserts sizes and a total error bound, is WHICH box the algorithm chooses to split
// next. Choosing by colour spread alone is drawn to the stray pixels lying between clusters: the box
// they form is very wide and nearly empty, so it wins split after split while a box holding half the
// image is never touched. This file pins the fix — spread weighted by population — with the numbers.
//
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * three tight clusters each get a palette entry of their own by K=5, including the one that
//     spread-alone selection dropped entirely even at K=8;
//   * mean error at K=8 is the noise floor of the data, not ten times it;
//   * error never rises as K grows, and identical input gives an identical palette.
#include "maz/render/ColorQuantize.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::render::mapToPalette;
using maz::render::nearestPaletteIndex;
using maz::render::quantizePalette;
using maz::render::Rgb8;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    // An integer in [-n, n].
    int jitter(int n) { return static_cast<int>(next() % static_cast<std::uint32_t>(2 * n + 1)) - n; }
};

static double dist(Rgb8 a, Rgb8 b) {
    const double dr = static_cast<double>(a.r) - b.r;
    const double dg = static_cast<double>(a.g) - b.g;
    const double db = static_cast<double>(a.b) - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

static double meanError(const std::vector<Rgb8>& px, const std::vector<Rgb8>& pal) {
    double sum = 0.0;
    const std::vector<std::size_t> idx = mapToPalette(px, pal);
    for (std::size_t i = 0; i < px.size(); ++i) {
        sum += dist(px[i], pal[idx[i]]);
    }
    return sum / static_cast<double>(px.size());
}

static Rgb8 clampRgb(int r, int g, int b) {
    auto c = [](int v) {
        return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    };
    return Rgb8{c(r), c(g), c(b)};
}

int main() {
    // Three clusters of a thousand pixels each, jittered by the +-3 that any real image has.
    const Rgb8 centres[3] = {{200, 40, 40}, {40, 180, 60}, {50, 60, 200}};
    std::vector<Rgb8> px;
    {
        Lcg rng{0xC0107u};
        for (int i = 0; i < 3000; ++i) {
            const Rgb8& c = centres[i % 3];
            px.push_back(clampRgb(c.r + rng.jitter(3), c.g + rng.jitter(3), c.b + rng.jitter(3)));
        }
    }

    // --- 1. Every cluster is represented, from K=5 on. ---
    // Not from K=3, and this is the algorithm being honest rather than failing: median cut halves one
    // box at a time, so separating three clusters takes more than three boxes. Measured here, the
    // error falls 46.2 (K=3) -> 22.7 (K=4) -> 3.6 (K=5), and it is at K=5 that the last cluster stops
    // sharing an entry with its neighbour.
    {
        const std::vector<Rgb8> pal = quantizePalette(px, 5);
        CHECK(pal.size() == 5, "K=5 yields five palette colours");
        for (int c = 0; c < 3; ++c) {
            const std::size_t j = nearestPaletteIndex(centres[c], pal);
            CHECK(dist(pal[j], centres[c]) < 6.0,
                  "each cluster centre has a palette entry within the jitter at K=5");
        }
    }

    // --- 2. At K=8, the error is the noise floor. ---
    // Spread-alone selection scored 22.8 here, with no entry anywhere near the green cluster and
    // four entries spent on near-duplicates of red and blue. The mean distance from a pixel to its
    // own cluster centre is about 3, so anything near 3 means every pixel found its own cluster.
    {
        const std::vector<Rgb8> pal = quantizePalette(px, 8);
        CHECK(pal.size() == 8, "K=8 yields eight palette colours");
        const double err = meanError(px, pal);
        CHECK(err < 5.0, "K=8 on three clusters quantizes to the noise floor, not ten times it");
        if (err >= 5.0) {
            std::printf("  mean error was %.2f\n", err);
        }
        for (int c = 0; c < 3; ++c) {
            const std::size_t j = nearestPaletteIndex(centres[c], pal);
            CHECK(dist(pal[j], centres[c]) < 4.0, "each cluster is matched closely at K=8");
        }
    }

    // --- 3. A blend entry may exist, but it must not be serving much of the image. ---
    // Some palette entries DO sit between clusters, at K=8 as at any K, and that is correct: the
    // stray pixels between clusters have to go somewhere, and the average of a box holding only
    // strays is legitimately between the clusters. The bug was never that such an entry exists, it
    // was that one of them held HALF THE IMAGE — 1500 of 3000 pixels sharing a colour that matched
    // neither of the two clusters inside it. So the property to assert is population, not position:
    // every entry that is far from all three centres serves only a sliver of the image.
    {
        const std::vector<Rgb8> pal = quantizePalette(px, 8);
        const std::vector<std::size_t> idx = mapToPalette(px, pal);
        std::vector<std::size_t> used(pal.size(), 0);
        for (const std::size_t i : idx) {
            ++used[i];
        }
        for (std::size_t i = 0; i < pal.size(); ++i) {
            double nearest = 1e9;
            for (int c = 0; c < 3; ++c) {
                nearest = std::min(nearest, dist(pal[i], centres[c]));
            }
            if (nearest >= 30.0) {
                CHECK(used[i] * 20 < px.size(),
                      "an entry that matches no cluster serves under 5% of the image");
            }
        }
    }

    // --- 4. More colours never cost accuracy, and the result is deterministic. ---
    {
        double prev = 1e9;
        bool monotone = true;
        for (std::size_t k : {2u, 3u, 4u, 5u, 8u, 16u, 32u}) {
            const double e = meanError(px, quantizePalette(px, k));
            if (e > prev + 1e-6) {
                monotone = false;
            }
            prev = e;
        }
        CHECK(monotone, "more palette colours never increase the quantization error");

        const std::vector<Rgb8> a = quantizePalette(px, 8);
        const std::vector<Rgb8> b = quantizePalette(px, 8);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i) {
            if (a[i] != b[i]) {
                same = false;
            }
        }
        CHECK(same, "identical inputs produce identical palettes");
    }

    // --- 5. A uniform ramp has no clusters to find, and must not be made worse. ---
    // Population weighting is a tie everywhere here, which is the point: it helps where there is
    // structure to exploit and changes nothing where there is not.
    {
        std::vector<Rgb8> ramp;
        for (int i = 0; i < 2000; ++i) {
            const int v = i * 255 / 1999;
            ramp.push_back(clampRgb(v, v, v));
        }
        CHECK(meanError(ramp, quantizePalette(ramp, 8)) < 15.0, "a grey ramp at K=8 stays close");
        CHECK(meanError(ramp, quantizePalette(ramp, 32)) < 4.0, "a grey ramp at K=32 is near exact");
    }

    if (g_fail == 0) {
        std::printf("colorquantize: OK — clusters kept, noise-floor error, monotone, deterministic.\n");
        return 0;
    }
    std::printf("colorquantize: %d failure(s).\n", g_fail);
    return 1;
}
