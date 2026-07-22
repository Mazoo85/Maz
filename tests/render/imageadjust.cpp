// tests/render/imageadjust.cpp — verifies the image tone/adjustment ops (render::adjustBrightness/adjustContrast/
// adjustGamma/invert/grayscale/threshold). Ground truths: brightness adds a clamped offset; contrast scales about
// 0.5 (factor 1 identity, 0 flat grey, >1 pushes extremes); gamma is v^g (g=1 identity, monotone); invert is 1-c;
// grayscale collapses to Rec.709 luminance (r==g==b); threshold makes a crisp two-colour mask on luminance; alpha
// is preserved and empty input is safe. Pure CPU, headless.
#include "maz/render/ImageAdjust.hpp"
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 2.0f / 255.0f; }
static bool colEq(const Color& c, float r, float g, float b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

int main() {
    // --- 1. Brightness adds a clamped offset; alpha preserved. ---
    {
        const Image src(4, 4, Color{0.4f, 0.5f, 0.6f, 0.7f});
        const Image up = adjustBrightness(src, 0.2f);
        CHECK(colEq(up.getPixel(0, 0), 0.6f, 0.7f, 0.8f), "brightness +0.2 lifts every channel");
        CHECK(near(up.getPixel(0, 0).a, 0.7f), "brightness preserves alpha");
        CHECK(colEq(adjustBrightness(src, 1.0f).getPixel(0, 0), 1, 1, 1), "brightness clamps at white");
        CHECK(colEq(adjustBrightness(src, -1.0f).getPixel(0, 0), 0, 0, 0), "brightness clamps at black");
    }

    // --- 2. Contrast about the 0.5 pivot. ---
    {
        const Image mid(2, 2, Color{0.5f, 0.5f, 0.5f, 1});
        CHECK(colEq(adjustContrast(mid, 3.0f).getPixel(0, 0), 0.5f, 0.5f, 0.5f), "contrast leaves the 0.5 pivot fixed");
        const Image lo(2, 2, Color{0.25f, 0.25f, 0.25f, 1});
        // (0.25-0.5)*2 + 0.5 = 0.0
        CHECK(colEq(adjustContrast(lo, 2.0f).getPixel(0, 0), 0, 0, 0), "contrast x2 pushes 0.25 down to 0");
        // factor 0 collapses everything to mid grey.
        CHECK(colEq(adjustContrast(Image(2, 2, Color{0.9f, 0.1f, 0.3f, 1}), 0.0f).getPixel(0, 0), 0.5f, 0.5f, 0.5f),
              "contrast 0 flattens to mid grey");
        // factor 1 is identity.
        CHECK(colEq(adjustContrast(lo, 1.0f).getPixel(0, 0), 0.25f, 0.25f, 0.25f), "contrast 1 is identity");
    }

    // --- 3. Gamma: g=1 identity; endpoints fixed; monotone (0.5^2 = 0.25). ---
    {
        const Image g(2, 2, Color{0.5f, 0.5f, 0.5f, 1});
        CHECK(colEq(adjustGamma(g, 1.0f).getPixel(0, 0), 0.5f, 0.5f, 0.5f), "gamma 1 is identity");
        CHECK(colEq(adjustGamma(g, 2.0f).getPixel(0, 0), 0.25f, 0.25f, 0.25f), "gamma 2 darkens mid to 0.25");
        CHECK(colEq(adjustGamma(Image(2, 2, Color{1, 0, 1, 1}), 2.0f).getPixel(0, 0), 1, 0, 1),
              "gamma leaves 0 and 1 endpoints fixed");
    }

    // --- 4. Invert = 1 - channel, alpha preserved. ---
    {
        const Image src(2, 2, Color{0.2f, 0.6f, 1.0f, 0.4f});
        const Image inv = invert(src);
        CHECK(colEq(inv.getPixel(0, 0), 0.8f, 0.4f, 0.0f), "invert = 1 - channel");
        CHECK(near(inv.getPixel(0, 0).a, 0.4f), "invert preserves alpha");
    }

    // --- 5. Grayscale collapses to Rec.709 luminance (equal channels). ---
    {
        const Image src(2, 2, Color{1, 0, 0, 1}); // pure red
        const Color g = grayscale(src).getPixel(0, 0);
        CHECK(near(g.r, g.g) && near(g.g, g.b), "grayscale makes r==g==b");
        CHECK(near(g.r, 0.2126f), "pure red maps to its Rec.709 luminance (~0.2126)");
    }

    // --- 6. Threshold: crisp two-colour mask on luminance. ---
    {
        // A black->white vertical ramp: the bottom half (brighter) crosses the 0.5 threshold.
        const Image ramp = patterns::verticalGradient(4, 16, Color{0, 0, 0, 1}, Color{1, 1, 1, 1});
        const Image mask = threshold(ramp, 0.5f);
        CHECK(colEq(mask.getPixel(0, 0), 0, 0, 0), "dark end below threshold -> low (black)");
        CHECK(colEq(mask.getPixel(0, 15), 1, 1, 1), "bright end above threshold -> high (white)");
        // custom colours.
        const Image cmask = threshold(ramp, 0.5f, Color{1, 0, 0, 1}, Color{0, 0, 1, 1});
        CHECK(colEq(cmask.getPixel(0, 0), 0, 0, 1) && colEq(cmask.getPixel(0, 15), 1, 0, 0),
              "threshold honours custom high/low colours");
    }

    // --- 7. Empty-safe. ---
    {
        CHECK(adjustBrightness(Image(), 0.2f).empty(), "brightness on empty -> empty");
        CHECK(grayscale(Image()).empty(), "grayscale on empty -> empty");
        CHECK(threshold(Image(), 0.5f).empty(), "threshold on empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("imageadjust: OK — brightness/contrast/gamma/invert/grayscale/threshold, alpha, safe.\n");
        return 0;
    }
    std::printf("imageadjust: %d failure(s).\n", g_fail);
    return 1;
}
