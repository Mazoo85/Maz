// tests/render/imageblend.cpp — verifies the image blend/composite modes (render::blend). Ground truths: each mode
// applies its per-channel maths (Multiply darkens / white is a no-op, Screen brightens / black is a no-op, Add
// clamps, Darken/Lighten pick min/max, Difference is |b-s|, Overlay passes mid-grey through); Normal honours the
// top pixel's alpha; `opacity` scales the whole layer (0 -> base unchanged); a smaller top only affects the
// overlap region; the base's own alpha is preserved; an empty base is safe. Pure CPU, headless.
#include "maz/render/ImageBlend.hpp"
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 2.0f / 255.0f; }
static bool colEq(const Color& c, float r, float g, float b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

static Image solid(int w, int h, float r, float g, float b, float a = 1.0f) {
    return Image(w, h, Color{r, g, b, a});
}

int main() {
    // --- 1. Multiply: base*top; white top is a no-op, black top -> black. ---
    {
        const Image base = solid(4, 4, 0.8f, 0.4f, 0.2f);
        CHECK(colEq(blend(base, solid(4, 4, 1, 1, 1), ImageBlendMode::Multiply).getPixel(0, 0), 0.8f, 0.4f, 0.2f),
              "multiply by white leaves base unchanged");
        CHECK(colEq(blend(base, solid(4, 4, 0, 0, 0), ImageBlendMode::Multiply).getPixel(0, 0), 0, 0, 0),
              "multiply by black -> black");
        CHECK(colEq(blend(base, solid(4, 4, 0.5f, 0.5f, 0.5f), ImageBlendMode::Multiply).getPixel(0, 0), 0.4f, 0.2f, 0.1f),
              "multiply by 0.5 halves each channel");
    }

    // --- 2. Screen: brightens; black top is a no-op, white -> white. ---
    {
        const Image base = solid(4, 4, 0.5f, 0.5f, 0.5f);
        CHECK(colEq(blend(base, solid(4, 4, 0, 0, 0), ImageBlendMode::Screen).getPixel(0, 0), 0.5f, 0.5f, 0.5f),
              "screen with black leaves base unchanged");
        CHECK(colEq(blend(base, solid(4, 4, 1, 1, 1), ImageBlendMode::Screen).getPixel(0, 0), 1, 1, 1),
              "screen with white -> white");
        // screen(0.5,0.5) = 1-(0.5)(0.5) = 0.75
        CHECK(colEq(blend(base, solid(4, 4, 0.5f, 0.5f, 0.5f), ImageBlendMode::Screen).getPixel(0, 0), 0.75f, 0.75f, 0.75f),
              "screen(0.5,0.5) = 0.75");
    }

    // --- 3. Add clamps; Darken/Lighten pick per-channel min/max; Difference is |b-s|. ---
    {
        const Image red = solid(2, 2, 1, 0, 0), grn = solid(2, 2, 0, 1, 0);
        CHECK(colEq(blend(red, grn, ImageBlendMode::Add).getPixel(0, 0), 1, 1, 0), "red + green (add) -> yellow");
        CHECK(colEq(blend(solid(2, 2, 0.7f, 0.7f, 0.7f), solid(2, 2, 0.7f, 0.7f, 0.7f), ImageBlendMode::Add).getPixel(0, 0),
                    1, 1, 1), "add clamps at 1");
        const Image a = solid(2, 2, 0.3f, 0.9f, 0.5f), b = solid(2, 2, 0.7f, 0.2f, 0.5f);
        CHECK(colEq(blend(a, b, ImageBlendMode::Darken).getPixel(0, 0), 0.3f, 0.2f, 0.5f), "darken = per-channel min");
        CHECK(colEq(blend(a, b, ImageBlendMode::Lighten).getPixel(0, 0), 0.7f, 0.9f, 0.5f), "lighten = per-channel max");
        CHECK(colEq(blend(a, b, ImageBlendMode::Difference).getPixel(0, 0), 0.4f, 0.7f, 0.0f), "difference = |b - s|");
    }

    // --- 4. Overlay passes mid-grey base through unchanged (2*0.5*s == s). ---
    {
        const Image base = solid(2, 2, 0.5f, 0.5f, 0.5f);
        CHECK(colEq(blend(base, solid(2, 2, 0.3f, 0.6f, 0.9f), ImageBlendMode::Overlay).getPixel(0, 0), 0.3f, 0.6f, 0.9f),
              "overlay over mid-grey base returns the top colour");
    }

    // --- 5. Normal honours top alpha; opacity scales the layer. ---
    {
        const Image blk = solid(4, 4, 0, 0, 0);
        // white top at alpha 0.5 over black -> mid grey.
        CHECK(colEq(blend(blk, solid(4, 4, 1, 1, 1, 0.5f), ImageBlendMode::Normal).getPixel(0, 0), 0.5f, 0.5f, 0.5f),
              "normal blend honours the top layer's alpha");
        // fully opaque white top but opacity 0 -> base unchanged.
        CHECK(colEq(blend(blk, solid(4, 4, 1, 1, 1), ImageBlendMode::Normal, 0.0f).getPixel(0, 0), 0, 0, 0),
              "opacity 0 leaves the base unchanged");
        // opacity 0.25 -> quarter toward white.
        CHECK(colEq(blend(blk, solid(4, 4, 1, 1, 1), ImageBlendMode::Normal, 0.25f).getPixel(0, 0), 0.25f, 0.25f, 0.25f),
              "opacity 0.25 mixes a quarter of the top");
    }

    // --- 6. A smaller top only affects the overlap; base alpha preserved; empty base safe. ---
    {
        const Image base = solid(8, 8, 0.2f, 0.2f, 0.2f, 0.6f);
        const Image top = solid(4, 4, 1, 1, 1); // covers only the top-left quarter
        const Image out = blend(base, top, ImageBlendMode::Normal);
        CHECK(colEq(out.getPixel(0, 0), 1, 1, 1), "top-left corner is covered by the top layer");
        CHECK(colEq(out.getPixel(7, 7), 0.2f, 0.2f, 0.2f), "outside the top layer the base shows through");
        CHECK(near(out.getPixel(0, 0).a, 0.6f) && near(out.getPixel(7, 7).a, 0.6f), "base alpha is preserved");
        CHECK(blend(Image(), top, ImageBlendMode::Normal).empty(), "empty base -> empty output");
    }

    // --- 7. Composability: two procedural patterns multiply into a valid, varied image. ---
    {
        const Image stone = patterns::noiseTexture(32, 32, 0.1f, 9, 4);
        const Image cells = patterns::checkerboard(32, 32, 4, Color{1, 1, 1, 1}, Color{0.5f, 0.5f, 0.5f, 1});
        const Image mixed = blend(stone, cells, ImageBlendMode::Multiply);
        CHECK(mixed.width() == 32 && mixed.height() == 32, "blend keeps the base size");
        float mn = 2, mx = -1;
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) { const float r = mixed.getPixel(x, y).r; mn = std::fmin(mn, r); mx = std::fmax(mx, r); }
        CHECK(mx - mn > 0.1f, "multiplying two patterns yields a varied image");
    }

    if (g_fail == 0) {
        std::printf("imageblend: OK — multiply/screen/add/darken/lighten/difference/overlay, alpha, opacity, overlap, safe.\n");
        return 0;
    }
    std::printf("imageblend: %d failure(s).\n", g_fail);
    return 1;
}
