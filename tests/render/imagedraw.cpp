// tests/render/imagedraw.cpp — verifies CPU image rasterizers (render::drawLine/drawRect/drawCircle/
// fillCircle/fillTriangle). Ground truths: a line sets its endpoints and a contiguous run of pixels and is
// safely clipped off-canvas; a rectangle outline sets its four borders but leaves the interior untouched; a
// circle outline hits the four cardinal points and leaves the centre empty; a filled disc lights the centre
// and cardinal points but not the far corner, and covers the right number of pixels; a filled triangle lights
// its interior and vertices but not points clearly outside it. Background/foreground chosen so 8-bit
// quantization is exact (black vs white). Pure CPU, deterministic, headless.
#include "maz/render/ImageDraw.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static const Color kBlack{0, 0, 0, 1};
static const Color kWhite{1, 1, 1, 1};

// "Set" means the pixel is white (drawn) rather than the black background.
static bool set(const Image& img, int x, int y) { return img.getPixel(x, y).r > 0.5f; }

static int countSet(const Image& img) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (set(img, x, y)) ++n;
    return n;
}

int main() {
    // --- 1. Line: endpoints + a contiguous horizontal run; off-canvas is clipped. ---
    {
        Image img(16, 16);
        img.fill(kBlack);
        drawLine(img, 2, 8, 12, 8, kWhite);
        CHECK(set(img, 2, 8) && set(img, 12, 8), "line endpoints set");
        bool run = true;
        for (int x = 2; x <= 12; ++x) if (!set(img, x, 8)) run = false;
        CHECK(run, "horizontal line is contiguous");
        CHECK(!set(img, 1, 8) && !set(img, 13, 8), "line does not overrun its endpoints");
        // A line partly off-canvas must not crash and still draws the on-canvas part.
        drawLine(img, -5, 0, 5, 0, kWhite);
        CHECK(set(img, 0, 0) && set(img, 5, 0), "off-canvas line clipped, visible part drawn");
    }

    // --- 2. Rectangle outline: borders set, interior empty. ---
    {
        Image img(16, 16);
        img.fill(kBlack);
        drawRect(img, 3, 3, 8, 6, kWhite); // corners (3,3)..(10,8)
        CHECK(set(img, 3, 3) && set(img, 10, 3) && set(img, 3, 8) && set(img, 10, 8), "rect corners set");
        CHECK(set(img, 6, 3) && set(img, 6, 8) && set(img, 3, 5) && set(img, 10, 5), "rect edges set");
        CHECK(!set(img, 6, 5), "rect interior is NOT filled");
    }

    // --- 3. Circle outline: cardinal points set, centre empty. ---
    {
        Image img(32, 32);
        img.fill(kBlack);
        drawCircle(img, 16, 16, 8, kWhite);
        CHECK(set(img, 24, 16) && set(img, 8, 16) && set(img, 16, 24) && set(img, 16, 8), "circle cardinal points");
        CHECK(!set(img, 16, 16), "circle centre is empty (outline only)");
    }

    // --- 4. Filled disc: centre + cardinals set, far corner not, and pixel count is disc-sized. ---
    {
        Image img(32, 32);
        img.fill(kBlack);
        fillCircle(img, 16, 16, 5, kWhite);
        CHECK(set(img, 16, 16) && set(img, 21, 16) && set(img, 16, 11), "disc centre and cardinal edge set");
        CHECK(!set(img, 21, 21), "disc corner (dist √50 > 5) not set");
        // Count pixels with x²+y² <= 25 in [-5,5]² == 81.
        CHECK(countSet(img) == 81, "radius-5 disc covers exactly 81 pixels");
    }

    // --- 5. Filled triangle: interior + vertices set, outside not. ---
    {
        Image img(32, 32);
        img.fill(kBlack);
        fillTriangle(img, 4, 4, 28, 8, 10, 28, kWhite);
        CHECK(set(img, 4, 4) && set(img, 28, 8) && set(img, 10, 28), "triangle vertices set");
        CHECK(set(img, 12, 12), "a clearly-interior point is filled");
        CHECK(!set(img, 26, 26) && !set(img, 1, 1), "points clearly outside are not filled");
    }

    if (g_fail == 0) {
        std::printf("imagedraw: OK — line/clip, rect outline, circle outline, filled disc count, filled triangle.\n");
        return 0;
    }
    std::printf("imagedraw: %d failure(s).\n", g_fail);
    return 1;
}
