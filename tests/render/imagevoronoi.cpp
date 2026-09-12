// tests/render/imagevoronoi.cpp — verifies the flat-colour Voronoi mosaic (render::patterns::voronoiTexture).
// Ground truths: size honoured; the image is made of FLAT cells (a large majority of horizontal neighbours share
// the exact same colour) with a small palette (far fewer distinct colours than pixels); the result is colourful
// (not grey); same seed reproduces and a different seed changes both layout and palette; non-positive size is safe.
// Pure CPU, headless.
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <set>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static std::uint32_t key(const Color& c) {
    auto q = [](float v) { return static_cast<std::uint32_t>(v * 255.0f + 0.5f) & 0xFFu; };
    return (q(c.r) << 16) | (q(c.g) << 8) | q(c.b);
}

int main() {
    // --- 1. Size + flat cells + small palette. ---
    {
        const Image v = patterns::voronoiTexture(64, 64, 0.08f, 123);
        CHECK(v.width() == 64 && v.height() == 64, "voronoi honours size");
        std::set<std::uint32_t> colors;
        int sameNeighbour = 0, totalNeighbour = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                colors.insert(key(v.getPixel(x, y)));
                if (x + 1 < 64) {
                    ++totalNeighbour;
                    if (key(v.getPixel(x, y)) == key(v.getPixel(x + 1, y))) ++sameNeighbour;
                }
            }
        CHECK(colors.size() < 64u * 64u / 8u, "the mosaic has a small palette (flat cells, not per-pixel noise)");
        CHECK(static_cast<double>(sameNeighbour) / totalNeighbour > 0.6, "most horizontal neighbours share a colour (flat regions)");
    }

    // --- 2. Colourful (not grey). ---
    {
        const Image v = patterns::voronoiTexture(48, 48, 0.1f, 5);
        int colourful = 0;
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x) {
                const Color c = v.getPixel(x, y);
                const float mx = std::fmax(c.r, std::fmax(c.g, c.b));
                const float mn = std::fmin(c.r, std::fmin(c.g, c.b));
                if (mx - mn > 0.1f) ++colourful;
            }
        CHECK(colourful > 48 * 48 / 2, "most cells are colourful (saturated hues, not grey)");
    }

    // --- 3. Deterministic by seed. ---
    {
        CHECK(patterns::voronoiTexture(32, 32, 0.1f, 7).data() == patterns::voronoiTexture(32, 32, 0.1f, 7).data(),
              "same seed reproduces the identical mosaic");
        CHECK(patterns::voronoiTexture(32, 32, 0.1f, 7).data() != patterns::voronoiTexture(32, 32, 0.1f, 8).data(),
              "a different seed changes layout & palette");
    }

    // --- 4. Empty-safe. ---
    {
        CHECK(patterns::voronoiTexture(0, 20).empty(), "zero width -> empty");
        CHECK(patterns::voronoiTexture(20, -1).empty(), "negative height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagevoronoi: OK — sized, flat cells, small palette, colourful, seed-deterministic, safe.\n");
        return 0;
    }
    std::printf("imagevoronoi: %d failure(s).\n", g_fail);
    return 1;
}
