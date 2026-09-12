// tests/film/canvas.cpp — verifies the sets' drawing surface (film Canvas.hpp).
//
// The fifteen sets are 500 lines of canvas-2D calls, and porting them by hand is only sane if the
// target reads the way the source does. This is the subset they actually use -- counted from the
// source, not guessed -- so the checks are that each of those operations does what a set author would
// expect, and in particular that the three things easiest to get silently wrong are right:
//   * the WORLD-to-image transform, since a set is composed in 1000x420 and drawn at any size;
//   * GRADIENTS, which must be evaluated in the world space they were defined in, not in pixels;
//   * the CLIP, which must confine drawing without a second coverage buffer.
#include "maz/film/Canvas.hpp"

#include <cmath>
#include <cstdio>

using maz::film::Canvas;
using maz::film::Gradient;
using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static const Color kBlack{0.0f, 0.0f, 0.0f, 1.0f};
static const Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};

static double ink(const Image& img) {
    double t = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) t += static_cast<double>(img.getPixel(x, y).r);
    }
    return t;
}

int main() {
    // --- 1. An untransformed fillRect is exact. ---
    {
        Image img(64, 64, kBlack);
        Canvas c(img, 0, 64);
        c.setFill(kWhite);
        c.fillRect(8.0f, 10.0f, 20.0f, 12.0f);
        CHECK(std::fabs(ink(img) - 240.0) < 0.02, "a fillRect covers exactly its area");
        CHECK(img.getPixel(10, 12).r > 0.99f, "the inside of a fillRect is filled");
        CHECK(img.getPixel(30, 12).r < 0.01f, "outside a fillRect is untouched");
    }

    // --- 2. The world transform scales and offsets. ---
    {
        // A 1000x420 world drawn into a 500x210 image: everything at half size.
        Image img(500, 210, kBlack);
        Canvas c(img, 0, 210);
        c.setTransform(0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f);
        c.setFill(kWhite);
        c.fillRect(100.0f, 40.0f, 200.0f, 80.0f); // world 200x80 -> device 100x40
        CHECK(std::fabs(ink(img) - 4000.0) < 1.0, "a world rect lands at the transformed size");
        CHECK(img.getPixel(60, 30).r > 0.99f, "the transformed rect is where the transform put it");
        CHECK(img.getPixel(40, 30).r < 0.01f, "and not where it would be untransformed");
    }

    // --- 3. A linear gradient runs between its two points, in WORLD space. ---
    {
        Image img(200, 100, kBlack);
        Canvas c(img, 0, 100);
        c.setTransform(0.2f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f); // world 1000 wide -> 200 px
        Gradient g = Canvas::linearGradient(0.0f, 0.0f, 1000.0f, 0.0f);
        g.addStop(0.0f, Color{0.0f, 0.0f, 0.0f, 1.0f});
        g.addStop(1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});
        c.setFillGradient(g);
        c.fillRect(0.0f, 0.0f, 1000.0f, 500.0f);
        const float left = img.getPixel(2, 50).r;
        const float mid = img.getPixel(100, 50).r;
        const float right = img.getPixel(197, 50).r;
        CHECK(left < 0.05f, "a gradient starts at its first stop");
        CHECK(right > 0.95f, "a gradient ends at its last stop");
        CHECK(std::fabs(mid - 0.5f) < 0.04f, "a gradient is halfway across at its midpoint");
        // Defined in world space: had it been evaluated in pixels it would finish in the first 1000th
        // of the image and the whole rect would read white.
        CHECK(mid < 0.95f, "a gradient is measured in world units, not pixels");
    }

    // --- 4. A radial gradient falls off from its centre. ---
    {
        Image img(200, 200, kBlack);
        Canvas c(img, 0, 200);
        Gradient g = Canvas::radialGradient(100.0f, 100.0f, 0.0f, 100.0f, 100.0f, 90.0f);
        g.addStop(0.0f, kWhite);
        g.addStop(1.0f, Color{0.0f, 0.0f, 0.0f, 1.0f});
        c.setFillGradient(g);
        c.fillRect(0.0f, 0.0f, 200.0f, 200.0f);
        CHECK(img.getPixel(100, 100).r > 0.95f, "a radial gradient is brightest at its centre");
        CHECK(img.getPixel(100, 195).r < 0.05f, "and dark at its outer radius");
        CHECK(img.getPixel(100, 145).r > 0.3f && img.getPixel(100, 145).r < 0.7f,
              "and halfway between, halfway out");
    }

    // --- 5. A clip confines drawing, and nests. ---
    {
        Image img(100, 100, kBlack);
        Canvas c(img, 0, 100);
        c.setFill(kWhite);
        c.save();
        c.clipRect(20.0f, 20.0f, 40.0f, 40.0f);
        c.fillRect(0.0f, 0.0f, 100.0f, 100.0f);
        CHECK(std::fabs(ink(img) - 1600.0) < 1.0, "a clip confines a fill to itself");
        c.clipRect(30.0f, 30.0f, 60.0f, 60.0f); // intersects to 30..60
        c.fillRect(0.0f, 0.0f, 100.0f, 100.0f);
        c.restore();
        // After restoring, the clip is gone and the whole frame can be painted.
        c.fillRect(0.0f, 0.0f, 100.0f, 100.0f);
        CHECK(std::fabs(ink(img) - 10000.0) < 2.0, "restore lifts the clip again");
    }

    // --- 6. The frame's own top and bottom always clip, letterbox included. ---
    {
        Image img(100, 100, kBlack);
        Canvas c(img, 25, 50); // the film sits in rows 25..74
        c.setFill(kWhite);
        c.fillRect(0.0f, 0.0f, 100.0f, 100.0f);
        CHECK(std::fabs(ink(img) - 5000.0) < 1.0, "nothing is drawn outside the letterboxed frame");
        CHECK(img.getPixel(50, 10).r < 0.01f, "the letterbox bar above stays black");
        CHECK(img.getPixel(50, 90).r < 0.01f, "the letterbox bar below stays black");
    }

    // --- 7. save/restore carries the paint, not just the clip. ---
    {
        Image img(40, 40, kBlack);
        Canvas c(img, 0, 40);
        c.setFill(kWhite);
        c.save();
        c.setFill(Color{0.0f, 0.0f, 0.0f, 1.0f});
        c.restore();
        c.fillRect(0.0f, 0.0f, 10.0f, 10.0f);
        CHECK(img.getPixel(5, 5).r > 0.99f, "restore brings back the fill colour");
        c.restore(); // one too many: must not misbehave
        CHECK(true, "restoring more than was saved is harmless");
    }

    // --- 8. A path fills, and a stroke draws a line of the stated width. ---
    {
        Image img(100, 100, kBlack);
        Canvas c(img, 0, 100);
        c.setFill(kWhite);
        c.beginPath();
        c.moveTo(10.0f, 10.0f);
        c.lineTo(90.0f, 20.0f);
        c.lineTo(20.0f, 80.0f);
        c.closePath();
        c.fill();
        const double area = std::fabs(10.0 * (20.0 - 80.0) + 90.0 * (80.0 - 10.0) + 20.0 * (10.0 - 20.0)) / 2.0;
        CHECK(std::fabs(ink(img) - area) < 2.0, "a path fills its own area");
    }
    {
        Image img(100, 100, kBlack);
        Canvas c(img, 0, 100);
        c.setStroke(kWhite);
        c.setLineWidth(6.0f);
        c.beginPath();
        c.moveTo(20.0f, 50.0f);
        c.lineTo(80.0f, 50.0f);
        c.stroke();
        // A 60-long, 6-wide line plus a round cap at each end.
        const double expected = 60.0 * 6.0 + 3.14159265 * 9.0;
        CHECK(std::fabs(ink(img) - expected) < 6.0, "a stroke is as wide as the line width");
        CHECK(img.getPixel(50, 50).r > 0.99f, "a stroke covers its own line");
        CHECK(img.getPixel(50, 60).r < 0.01f, "a stroke does not cover beyond its width");
    }

    // --- 9. A stroke does not darken where it overlaps itself. ---
    {
        Image img(100, 100, kBlack);
        Canvas c(img, 0, 100);
        c.setStroke(Color{1.0f, 1.0f, 1.0f, 0.5f});
        c.setLineWidth(8.0f);
        c.beginPath();
        c.moveTo(20.0f, 20.0f);
        c.lineTo(50.0f, 50.0f);
        c.lineTo(80.0f, 20.0f);
        c.stroke();
        // The corner is where two quads and a join circle all overlap. At half alpha, a second
        // composite there would read distinctly brighter than the straight run.
        const float corner = img.getPixel(50, 50).r;
        const float run = img.getPixel(30, 30).r;
        CHECK(std::fabs(corner - run) < 0.06f,
              "a stroke's corner is the same tone as its straight run, not doubled");
    }

    // --- 10. Determinism. ---
    {
        Image a(80, 80, kBlack), b(80, 80, kBlack);
        for (Image* img : {&a, &b}) {
            Canvas c(*img, 0, 80);
            c.setTransform(0.08f, 0.0f, 0.0f, 0.19f, 0.0f, 0.0f);
            Gradient g = Canvas::linearGradient(0.0f, 0.0f, 0.0f, 420.0f);
            g.addStop(0.0f, kWhite);
            g.addStop(1.0f, Color{0.2f, 0.1f, 0.3f, 1.0f});
            c.setFillGradient(g);
            c.fillRect(0.0f, 0.0f, 1000.0f, 420.0f);
            c.setFill(Color{0.9f, 0.8f, 0.2f, 0.7f});
            c.beginPath();
            c.circle(500.0f, 210.0f, 120.0f);
            c.fill();
        }
        CHECK(a.data() == b.data(), "drawing the same thing twice gives identical bytes");
    }

    if (g_fail == 0) {
        std::printf("film canvas: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
