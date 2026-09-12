// tests/render/imagepatterns.cpp — verifies procedural image patterns (render::patterns). Ground truths: a
// checkerboard alternates the two colours every `cell` pixels from the top-left; a vertical gradient hits `top` on
// the first row, `bottom` on the last, and the midpoint halfway between; a radial gradient is `centre` at the middle
// and reaches `edge` by the shorter-side radius; sizes are honoured and non-positive sizes give an empty image.
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool colClose(const Color& c, float r, float g, float b, float a) {
    const float tol = 2.0f / 255.0f; // 8-bit quantisation
    return std::fabs(c.r - r) <= tol && std::fabs(c.g - g) <= tol && std::fabs(c.b - b) <= tol && std::fabs(c.a - a) <= tol;
}

int main() {
    const Color white{1, 1, 1, 1}, black{0, 0, 0, 1};

    // --- 1. Checkerboard: cell 2, alternates from the top-left. ---
    {
        const Image img = patterns::checkerboard(8, 8, 2, white, black);
        CHECK(img.width() == 8 && img.height() == 8, "checkerboard honours the requested size");
        CHECK(colClose(img.getPixel(0, 0), 1, 1, 1, 1), "cell (0,0) is colour A (white)");
        CHECK(colClose(img.getPixel(2, 0), 0, 0, 0, 1), "next cell across is colour B (black)");
        CHECK(colClose(img.getPixel(0, 2), 0, 0, 0, 1), "next cell down is colour B (black)");
        CHECK(colClose(img.getPixel(2, 2), 1, 1, 1, 1), "diagonal cell returns to colour A (white)");
        CHECK(colClose(img.getPixel(1, 1), 1, 1, 1, 1), "within a cell the colour is constant");
    }

    // --- 2. Vertical gradient: top row = top, bottom row = bottom, middle = halfway. ---
    {
        const Image img = patterns::verticalGradient(4, 3, black, white);
        CHECK(colClose(img.getPixel(0, 0), 0, 0, 0, 1), "top row is the top colour");
        CHECK(colClose(img.getPixel(0, 2), 1, 1, 1, 1), "bottom row is the bottom colour");
        CHECK(colClose(img.getPixel(0, 1), 0.5f, 0.5f, 0.5f, 1), "middle row is halfway (grey)");
        // Rows are constant across x.
        CHECK(colClose(img.getPixel(3, 1), 0.5f, 0.5f, 0.5f, 1), "gradient is constant along each row");
    }

    // --- 3. Radial gradient: centre pixel ~ centre colour, a corner ~ edge colour. ---
    {
        const Image img = patterns::radialGradient(64, 64, white, black);
        // The exact centre falls between pixels, so the nearest pixel is very close to (not exactly) the centre colour.
        CHECK(img.getPixel(32, 32).r > 0.95f, "centre pixel is near the centre colour (white)");
        CHECK(colClose(img.getPixel(0, 0), 0, 0, 0, 1), "a corner (beyond the radius) is the edge colour");
        // A pixel partway out is a partial blend (between the two), and darker than the centre.
        const Color mid = img.getPixel(32 + 16, 32);
        CHECK(mid.r > 0.1f && mid.r < 0.9f, "a mid-radius pixel is a partial blend");
        CHECK(mid.r < img.getPixel(32, 32).r, "the gradient darkens outward from the centre");
    }

    // --- 4. Non-positive size -> empty image. ---
    {
        CHECK(patterns::checkerboard(0, 8, 2, white, black).empty(), "zero width -> empty");
        CHECK(patterns::verticalGradient(8, -1, white, black).empty(), "negative height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagepatterns: OK — checker alternation, gradient endpoints+mid, radial centre/edge, safe.\n");
        return 0;
    }
    std::printf("imagepatterns: %d failure(s).\n", g_fail);
    return 1;
}
