// tests/render/imagegradientmap.cpp — verifies the gradient-map colorizer (render::gradientMap). Ground truths:
// size is honoured; the two-colour form maps dark luminance -> lo and bright luminance -> hi (a black->white input
// ramp comes out lo->hi); the multi-stop form clamps past the ends and interpolates linearly between stops (mid
// grey through a black/red/white ramp is red); the callback form runs an arbitrary Color(float); the result is
// deterministic; a grey noise image colorizes into a varied colour image (composability); empty input is safe.
// Pure CPU, headless.
#include "maz/render/ImageGradientMap.hpp"
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 3.0f / 255.0f; }
static bool colEq(const Color& c, float r, float g, float b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

int main() {
    // A black(top)->white(bottom) grey ramp: luminance runs 0 at row 0 to 1 at the last row.
    const Image ramp = patterns::verticalGradient(8, 32, Color{0, 0, 0, 1}, Color{1, 1, 1, 1});

    // --- 1. Two-colour form: size honoured; top ~ lo, bottom ~ hi. ---
    {
        const Color lo{1, 0, 0, 1}, hi{0, 1, 0, 1}; // red -> green
        const Image out = gradientMap(ramp, lo, hi);
        CHECK(out.width() == 8 && out.height() == 32, "gradientMap honours the source size");
        CHECK(colEq(out.getPixel(0, 0), 1, 0, 0), "dark luminance maps to lo (red)");
        CHECK(colEq(out.getPixel(0, 31), 0, 1, 0), "bright luminance maps to hi (green)");
        // A mid row should be a blend (both channels present).
        const Color mid = out.getPixel(0, 16);
        CHECK(mid.r > 0.2f && mid.g > 0.2f, "mid luminance blends lo and hi");
    }

    // --- 2. Multi-stop form: clamp past ends + linear interior; mid grey -> red. ---
    {
        const std::vector<ColorStop> stops = {
            {0.0f, Color{0, 0, 0, 1}}, {0.5f, Color{1, 0, 0, 1}}, {1.0f, Color{1, 1, 1, 1}}};
        // Solid mid-grey (luminance 0.5) -> exactly the middle stop (red).
        const Image grey(4, 4, Color{0.5f, 0.5f, 0.5f, 1.0f});
        const Image out = gradientMap(grey, stops);
        CHECK(colEq(out.getPixel(2, 2), 1, 0, 0), "mid grey hits the middle stop (red)");
        // Clamp: pure black -> first stop, pure white -> last stop.
        const Image blk(2, 2, Color{0, 0, 0, 1});
        const Image wht(2, 2, Color{1, 1, 1, 1});
        CHECK(colEq(gradientMap(blk, stops).getPixel(0, 0), 0, 0, 0), "below first stop clamps to first colour");
        CHECK(colEq(gradientMap(wht, stops).getPixel(0, 0), 1, 1, 1), "above last stop clamps to last colour");
        // A quarter-grey (0.25) sits halfway between stop0(black) and stop1(red) -> (0.5,0,0).
        const Image q(2, 2, Color{0.25f, 0.25f, 0.25f, 1.0f});
        CHECK(colEq(gradientMap(q, stops).getPixel(0, 0), 0.5f, 0, 0), "quarter grey interpolates between stops");
    }

    // --- 3. Callback form runs an arbitrary Color(float). ---
    {
        const Image out = gradientMap(ramp, [](float t) { return Color{t, t * t, 0.0f, 1.0f}; });
        CHECK(colEq(out.getPixel(0, 0), 0, 0, 0), "callback at t=0 -> black");
        CHECK(colEq(out.getPixel(0, 31), 1, 1, 0), "callback at t=1 -> (1,1,0)");
    }

    // --- 4. Deterministic + empty-safe. ---
    {
        const Image a = gradientMap(ramp, Color{1, 0, 0, 1}, Color{0, 0, 1, 1});
        const Image b = gradientMap(ramp, Color{1, 0, 0, 1}, Color{0, 0, 1, 1});
        CHECK(a.data() == b.data(), "same input reproduces an identical colorized image");
        CHECK(gradientMap(Image(), Color{1, 1, 1, 1}, Color{0, 0, 0, 1}).empty(), "empty input -> empty output");
    }

    // --- 5. Composability: grey noise -> a varied COLOUR image (not a flat fill). ---
    {
        const Image noise = patterns::noiseTexture(48, 48, 0.12f, 42, 4);
        const std::vector<ColorStop> terrain = {{0.0f, Color{0.1f, 0.2f, 0.6f, 1}}, // deep water
                                                {0.4f, Color{0.8f, 0.7f, 0.4f, 1}}, // sand
                                                {0.7f, Color{0.2f, 0.6f, 0.2f, 1}}, // grass
                                                {1.0f, Color{1, 1, 1, 1}}};         // snow
        const Image col = gradientMap(noise, terrain);
        float rmn = 2, rmx = -1, bmn = 2, bmx = -1;
        for (int y = 0; y < col.height(); ++y)
            for (int x = 0; x < col.width(); ++x) {
                const Color c = col.getPixel(x, y);
                rmn = std::fmin(rmn, c.r); rmx = std::fmax(rmx, c.r);
                bmn = std::fmin(bmn, c.b); bmx = std::fmax(bmx, c.b);
            }
        CHECK(rmx - rmn > 0.1f && bmx - bmn > 0.1f, "colorized noise varies across the image (real colour range)");
    }

    if (g_fail == 0) {
        std::printf("imagegradientmap: OK — sized, two-colour, multi-stop clamp/lerp, callback, deterministic, safe.\n");
        return 0;
    }
    std::printf("imagegradientmap: %d failure(s).\n", g_fail);
    return 1;
}
