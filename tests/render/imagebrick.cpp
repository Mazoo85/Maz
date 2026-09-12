// tests/render/imagebrick.cpp — verifies the running-bond brick pattern (render::patterns::brickWall). Ground
// truths: size is honoured; both the brick and mortar colours appear; horizontal mortar lines sit in the gap band
// (ly in [brickH, brickH+mortar)); the wall is running-bond — the vertical mortar columns of a row are shifted
// relative to the row above (adjacent rows differ); a zero/negative size is safe; sub-1 sizes are clamped to 1.
// Pure CPU, headless.
#include "maz/render/ImageAdjust.hpp"   // grayscale (composability)
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 2.0f / 255.0f; }
static bool colEq(const Color& c, float r, float g, float b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

int main() {
    const Color brick{0.6f, 0.2f, 0.15f, 1};
    const Color mortar{0.8f, 0.8f, 0.8f, 1};

    // --- 1. Size honoured; both colours present. ---
    {
        const Image w = patterns::brickWall(64, 48, 16, 8, 2, brick, mortar, 0.5f);
        CHECK(w.width() == 64 && w.height() == 48, "brick wall honours the requested size");
        bool sawBrick = false, sawMortar = false;
        for (int y = 0; y < w.height(); ++y)
            for (int x = 0; x < w.width(); ++x) {
                const Color c = w.getPixel(x, y);
                if (colEq(c, brick.r, brick.g, brick.b)) sawBrick = true;
                if (colEq(c, mortar.r, mortar.g, mortar.b)) sawMortar = true;
            }
        CHECK(sawBrick && sawMortar, "both brick and mortar colours appear");
    }

    // --- 2. Horizontal mortar lines sit in the gap band (rows brickH..brickH+mortar are all mortar). ---
    {
        const Image w = patterns::brickWall(64, 48, 16, 8, 2, brick, mortar, 0.5f);
        // periodY = 8 + 2 = 10; the mortar band of the first row is y in [8, 10).
        bool band = true;
        for (int x = 0; x < 64; ++x)
            if (!colEq(w.getPixel(x, 8), mortar.r, mortar.g, mortar.b) ||
                !colEq(w.getPixel(x, 9), mortar.r, mortar.g, mortar.b)) band = false;
        CHECK(band, "the full horizontal mortar band is mortar-coloured");
        // A brick interior row (y=3) has at least some brick pixels.
        bool anyBrick = false;
        for (int x = 0; x < 64; ++x) if (colEq(w.getPixel(x, 3), brick.r, brick.g, brick.b)) anyBrick = true;
        CHECK(anyBrick, "a brick-interior row contains brick pixels");
    }

    // --- 3. Running bond: the vertical mortar columns differ between adjacent brick rows. ---
    {
        const Image w = patterns::brickWall(64, 48, 16, 8, 2, brick, mortar, 0.5f);
        // Row 0 brick-interior is y=3; row 1 brick-interior is y=13 (periodY=10). Compare their mortar-column masks.
        bool differs = false;
        for (int x = 0; x < 64; ++x) {
            const bool m0 = colEq(w.getPixel(x, 3), mortar.r, mortar.g, mortar.b);
            const bool m1 = colEq(w.getPixel(x, 13), mortar.r, mortar.g, mortar.b);
            if (m0 != m1) { differs = true; break; }
        }
        CHECK(differs, "adjacent rows are staggered (running bond)");
    }

    // --- 4. offsetFrac 0 -> stacked bond: adjacent rows share the SAME vertical mortar columns. ---
    {
        const Image w = patterns::brickWall(64, 48, 16, 8, 2, brick, mortar, 0.0f);
        bool same = true;
        for (int x = 0; x < 64; ++x) {
            const bool m0 = colEq(w.getPixel(x, 3), mortar.r, mortar.g, mortar.b);
            const bool m1 = colEq(w.getPixel(x, 13), mortar.r, mortar.g, mortar.b);
            if (m0 != m1) { same = false; break; }
        }
        CHECK(same, "offsetFrac 0 gives a stacked bond (columns line up)");
    }

    // --- 5. Composability: grayscale a brick wall into a height source (still two distinct greys). ---
    {
        const Image g = grayscale(patterns::brickWall(32, 32, 8, 4, 1, brick, mortar, 0.5f));
        float mn = 2, mx = -1;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) { const float r = g.getPixel(x, y).r; mn = std::fmin(mn, r); mx = std::fmax(mx, r); }
        CHECK(mx - mn > 0.1f, "a greyscale brick wall keeps brick/mortar contrast (usable as a height map)");
    }

    // --- 6. Safe / clamped inputs. ---
    {
        CHECK(patterns::brickWall(0, 16, 8, 4, 1, brick, mortar).empty(), "zero width -> empty");
        CHECK(!patterns::brickWall(8, 8, 0, 0, 0, brick, mortar).empty(), "sub-1 brick/mortar sizes are clamped, not fatal");
    }

    if (g_fail == 0) {
        std::printf("imagebrick: OK — sized, both colours, mortar band, running bond, stacked bond, composable, safe.\n");
        return 0;
    }
    std::printf("imagebrick: %d failure(s).\n", g_fail);
    return 1;
}
