// tests/render/imagemarblewood.cpp — verifies the domain-warped marble & wood textures
// (render::patterns::marbleTexture / woodTexture). Ground truths: size honoured; grey pixels in [0,1]; the field
// varies; same seed reproduces, different seed changes; with turbulence 0 the structure is exact — marble bands are
// vertical (every column is constant down its rows) and wood rings are concentric (equal radius from centre → equal
// value); turbulence > 0 breaks that perfect structure (warps it); non-positive size is safe. Pure CPU, headless.
#include "maz/render/ImageAdjust.hpp"   // grayscale check helper reuse not needed; kept minimal
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool greyInRange(const Image& img) {
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const Color c = img.getPixel(x, y);
            if (std::fabs(c.r - c.g) > 2.0f / 255.0f || std::fabs(c.r - c.b) > 2.0f / 255.0f) return false;
            if (c.r < -1e-4f || c.r > 1.0f + 1e-4f) return false;
        }
    return true;
}
static float range(const Image& img) {
    float mn = 2, mx = -1;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) { const float r = img.getPixel(x, y).r; mn = std::fmin(mn, r); mx = std::fmax(mx, r); }
    return mx - mn;
}

int main() {
    // --- 1. Marble: sized, grey, varies, deterministic. ---
    {
        const Image m = patterns::marbleTexture(64, 48, 0.15f, 4.0f, 0.05f, 7, 4);
        CHECK(m.width() == 64 && m.height() == 48, "marble honours size");
        CHECK(greyInRange(m), "marble is grey and in [0,1]");
        CHECK(range(m) > 0.2f, "marble field varies");
        CHECK(patterns::marbleTexture(32, 32, 0.15f, 4, 0.05f, 7, 4).data() ==
              patterns::marbleTexture(32, 32, 0.15f, 4, 0.05f, 7, 4).data(), "same seed -> identical marble");
        CHECK(patterns::marbleTexture(32, 32, 0.15f, 4, 0.05f, 7, 4).data() !=
              patterns::marbleTexture(32, 32, 0.15f, 4, 0.05f, 9, 4).data(), "different seed -> different marble");
    }

    // --- 2. Marble with turbulence 0: perfectly vertical bands (each column constant down its rows). ---
    {
        const Image m = patterns::marbleTexture(40, 24, 0.2f, 0.0f, 0.05f, 1, 4);
        bool columnsConstant = true;
        for (int x = 0; x < 40 && columnsConstant; ++x) {
            const float top = m.getPixel(x, 0).r;
            for (int y = 1; y < 24; ++y)
                if (std::fabs(m.getPixel(x, y).r - top) > 2.0f / 255.0f) { columnsConstant = false; break; }
        }
        CHECK(columnsConstant, "turbulence 0 -> straight vertical marble bands (columns constant)");
        // And with turbulence the columns are no longer constant.
        const Image mt = patterns::marbleTexture(40, 24, 0.2f, 5.0f, 0.08f, 1, 4);
        bool someColumnVaries = false;
        for (int x = 0; x < 40 && !someColumnVaries; ++x) {
            const float top = mt.getPixel(x, 0).r;
            for (int y = 1; y < 24; ++y)
                if (std::fabs(mt.getPixel(x, y).r - top) > 4.0f / 255.0f) { someColumnVaries = true; break; }
        }
        CHECK(someColumnVaries, "turbulence > 0 warps the bands (columns vary)");
    }

    // --- 3. Wood: sized, grey, varies, deterministic. ---
    {
        const Image w = patterns::woodTexture(64, 64, 0.6f, 3.0f, 0.04f, 3, 4);
        CHECK(w.width() == 64 && w.height() == 64, "wood honours size");
        CHECK(greyInRange(w), "wood is grey and in [0,1]");
        CHECK(range(w) > 0.2f, "wood field varies");
        CHECK(patterns::woodTexture(48, 48, 0.6f, 3, 0.04f, 3, 4).data() !=
              patterns::woodTexture(48, 48, 0.6f, 3, 0.04f, 8, 4).data(), "different seed -> different wood");
    }

    // --- 4. Wood with turbulence 0: concentric rings (equal radius from centre -> equal value). ---
    {
        const Image w = patterns::woodTexture(64, 64, 0.7f, 0.0f, 0.04f, 1, 4);
        // Four points at radius ~16 from the centre (32,32): (48,32),(16,32),(32,48),(32,16).
        const float a = w.getPixel(48, 32).r, b = w.getPixel(16, 32).r;
        const float c = w.getPixel(32, 48).r, d = w.getPixel(32, 16).r;
        CHECK(std::fabs(a - b) < 3.0f / 255.0f && std::fabs(a - c) < 3.0f / 255.0f &&
              std::fabs(a - d) < 3.0f / 255.0f, "turbulence 0 -> concentric rings (equal radius, equal value)");
    }

    // --- 5. Empty-safe. ---
    {
        CHECK(patterns::marbleTexture(0, 10).empty(), "marble zero width -> empty");
        CHECK(patterns::woodTexture(10, 0).empty(), "wood zero height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagemarblewood: OK — marble bands, wood rings, turbulence warps, grey/ranged, deterministic, safe.\n");
        return 0;
    }
    std::printf("imagemarblewood: %d failure(s).\n", g_fail);
    return 1;
}
