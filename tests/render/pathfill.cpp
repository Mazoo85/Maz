// tests/render/pathfill.cpp — verifies the anti-aliased vector path rasterizer (render Path.hpp,
// PathFill.hpp). The engine's CPU raster could draw a line, a rect, a circle outline and a triangle
// and nothing else; this is the primitive that fills an arbitrary shape, so it is tested against
// analytic ground truths rather than against a picture:
//   * an axis-aligned rect on pixel boundaries is exactly full inside and exactly empty outside;
//   * total ink (summed coverage) equals a shape's true area — triangle, circle, ellipse;
//   * winding direction does not change a nonzero fill;
//   * a ring is hollow under even-odd and solid under nonzero;
//   * a flattened curve stays within the requested tolerance of the true curve;
//   * nothing is ever written outside the path's bounding box, including for paths off-image;
//   * the same path filled twice gives identical pixels (determinism).
#include "maz/render/PathFill.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::render::Color;
using maz::render::FillRule;
using maz::render::Image;
using maz::render::Path;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static const Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
static const Color kBlack{0.0f, 0.0f, 0.0f, 1.0f};

// Coverage read back from a white-on-black fill = the red channel as 0..1.
static float cov(const Image& img, int x, int y) { return img.getPixel(x, y).r; }

// Total ink in the image, in pixels' worth of coverage.
static double ink(const Image& img) {
    double t = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            t += static_cast<double>(cov(img, x, y));
        }
    }
    return t;
}

int main() {
    // --- 1. A pixel-aligned rect is exact: full inside, empty outside. ---
    {
        Image img(40, 40, kBlack);
        Path p;
        p.rect(10.0f, 8.0f, 20.0f, 24.0f);
        maz::render::fillPath(img, p, kWhite);
        bool inFull = true, outClean = true;
        for (int y = 8; y < 32; ++y) {
            for (int x = 10; x < 30; ++x) {
                if (cov(img, x, y) < 0.999f) inFull = false;
            }
        }
        for (int y = 0; y < 40; ++y) {
            for (int x = 0; x < 40; ++x) {
                const bool inside = (x >= 10 && x < 30 && y >= 8 && y < 32);
                if (!inside && cov(img, x, y) > 0.001f) outClean = false;
            }
        }
        CHECK(inFull, "pixel-aligned rect is fully covered inside");
        CHECK(outClean, "pixel-aligned rect writes nothing outside itself");
        CHECK(std::fabs(ink(img) - 480.0) < 0.01, "rect ink equals its exact area (20x24)");
    }

    // --- 2. A half-pixel-offset rect splits its edge coverage evenly. ---
    {
        Image img(40, 40, kBlack);
        Path p;
        p.rect(10.5f, 10.0f, 9.0f, 10.0f);   // 9 x 10 = 90 area, straddling x=10 and x=19
        maz::render::fillPath(img, p, kWhite);
        CHECK(std::fabs(ink(img) - 90.0) < 0.05, "offset rect ink still equals its true area");
        CHECK(std::fabs(cov(img, 10, 12) - 0.5f) < 0.02f, "a half-covered edge pixel reads ~0.5");
        CHECK(std::fabs(cov(img, 19, 12) - 0.5f) < 0.02f, "the far half-covered edge pixel reads ~0.5");
    }

    // --- 3. Triangle ink equals its analytic area. ---
    {
        Image img(64, 64, kBlack);
        Path p;
        p.moveTo(8.0f, 8.0f).lineTo(56.0f, 20.0f).lineTo(24.0f, 56.0f).close();
        maz::render::fillPath(img, p, kWhite);
        // Shoelace: |x0(y1-y2) + x1(y2-y0) + x2(y0-y1)| / 2
        const double area = std::fabs(8.0 * (20.0 - 56.0) + 56.0 * (56.0 - 8.0) + 24.0 * (8.0 - 20.0)) / 2.0;
        CHECK(std::fabs(ink(img) - area) < 1.0, "triangle ink equals its shoelace area");
    }

    // --- 4. Circle ink equals pi*r^2, and an ellipse ink equals pi*rx*ry. ---
    {
        Image img(96, 96, kBlack);
        Path p;
        p.ellipse(48.0f, 48.0f, 30.0f, 30.0f);
        maz::render::fillPath(img, p, kWhite);
        const double area = 3.14159265358979 * 30.0 * 30.0;
        CHECK(std::fabs(ink(img) - area) < 3.0, "circle ink equals pi*r^2");
    }
    {
        Image img(96, 96, kBlack);
        Path p;
        p.ellipse(48.0f, 48.0f, 36.0f, 18.0f);
        maz::render::fillPath(img, p, kWhite);
        const double area = 3.14159265358979 * 36.0 * 18.0;
        CHECK(std::fabs(ink(img) - area) < 3.0, "ellipse ink equals pi*rx*ry");
    }

    // --- 5. Winding direction does not change a nonzero fill. ---
    {
        Image a(64, 64, kBlack), b(64, 64, kBlack);
        Path fwd, rev;
        fwd.moveTo(10.0f, 10.0f).lineTo(50.0f, 14.0f).lineTo(44.0f, 52.0f).lineTo(12.0f, 40.0f).close();
        rev.moveTo(12.0f, 40.0f).lineTo(44.0f, 52.0f).lineTo(50.0f, 14.0f).lineTo(10.0f, 10.0f).close();
        maz::render::fillPath(a, fwd, kWhite);
        maz::render::fillPath(b, rev, kWhite);
        bool same = true;
        for (int y = 0; y < 64 && same; ++y) {
            for (int x = 0; x < 64; ++x) {
                if (std::fabs(cov(a, x, y) - cov(b, x, y)) > 0.002f) { same = false; break; }
            }
        }
        CHECK(same, "reversing a contour's winding does not change a nonzero fill");
    }

    // --- 6. A ring: hollow under even-odd, solid under nonzero (same-wound contours). ---
    {
        Path ring;
        ring.ellipse(48.0f, 48.0f, 32.0f, 32.0f);
        ring.ellipse(48.0f, 48.0f, 16.0f, 16.0f);   // same winding direction

        Image eo(96, 96, kBlack);
        maz::render::fillPath(eo, ring, kWhite, FillRule::EvenOdd);
        CHECK(cov(eo, 48, 48) < 0.01f, "even-odd leaves the ring's hole empty");
        CHECK(cov(eo, 48, 24) > 0.99f, "even-odd fills the ring's band");
        const double band = 3.14159265358979 * (32.0 * 32.0 - 16.0 * 16.0);
        CHECK(std::fabs(ink(eo) - band) < 4.0, "even-odd ring ink equals the annulus area");

        Image nz(96, 96, kBlack);
        maz::render::fillPath(nz, ring, kWhite, FillRule::NonZero);
        CHECK(cov(nz, 48, 48) > 0.99f, "nonzero fills the hole when both contours wind the same way");
    }

    // --- 7. A reversed inner contour makes a hole under nonzero too. ---
    {
        Path ring;
        ring.ellipse(48.0f, 48.0f, 32.0f, 32.0f);
        ring.ellipse(48.0f, 48.0f, 16.0f, 16.0f, /*rotation*/ 0.0f, /*clockwise*/ true);
        Image nz(96, 96, kBlack);
        maz::render::fillPath(nz, ring, kWhite, FillRule::NonZero);
        CHECK(nz.getPixel(48, 48).r < 0.01f, "nonzero leaves a hole when the inner contour is reversed");
    }

    // --- 8. Curve flattening stays inside the requested tolerance. ---
    {
        Path p;
        p.setTolerance(0.05f);
        p.moveTo(10.0f, 50.0f).quadTo(30.0f, 0.0f, 50.0f, 50.0f).close();
        const auto& cs = p.contours();
        CHECK(cs.size() == 1, "a quadratic makes one contour");
        // Sample the true quadratic and check each sample is near some flattened segment.
        double worst = 0.0;
        for (int i = 0; i <= 200; ++i) {
            const double t = static_cast<double>(i) / 200.0;
            const double mt = 1.0 - t;
            const double bx = mt * mt * 10.0 + 2.0 * mt * t * 30.0 + t * t * 50.0;
            const double by = mt * mt * 50.0 + 2.0 * mt * t * 0.0 + t * t * 50.0;
            double best = 1e9;
            const auto& c = cs[0];
            for (std::size_t s = 0; s + 1 < c.size(); ++s) {
                const double ax = static_cast<double>(c[s].x), ay = static_cast<double>(c[s].y);
                const double cx2 = static_cast<double>(c[s + 1].x), cy2 = static_cast<double>(c[s + 1].y);
                const double dx = cx2 - ax, dy = cy2 - ay;
                const double len2 = dx * dx + dy * dy;
                double u = len2 > 0.0 ? ((bx - ax) * dx + (by - ay) * dy) / len2 : 0.0;
                u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
                const double px = ax + u * dx - bx, py = ay + u * dy - by;
                const double d = std::sqrt(px * px + py * py);
                if (d < best) best = d;
            }
            if (best > worst) worst = best;
        }
        CHECK(worst <= 0.05 + 1e-6, "a flattened quadratic stays within its tolerance of the true curve");
    }

    // --- 9. A path partly off-image is clipped, not wrapped or crashed. ---
    {
        Image img(32, 32, kBlack);
        Path p;
        p.rect(-20.0f, -20.0f, 30.0f, 30.0f);   // only the 10x10 corner lands
        maz::render::fillPath(img, p, kWhite);
        CHECK(std::fabs(ink(img) - 100.0) < 0.05, "an off-image path contributes only its visible part");
        CHECK(cov(img, 15, 15) < 0.001f, "an off-image path does not wrap into the far side");
    }
    {
        Image img(32, 32, kBlack);
        Path p;
        p.rect(100.0f, 100.0f, 10.0f, 10.0f);   // entirely off-image
        maz::render::fillPath(img, p, kWhite);
        CHECK(ink(img) < 0.001, "a fully off-image path draws nothing");
    }

    // --- 10. Determinism: the same path filled twice is pixel-identical. ---
    {
        Image a(64, 64, kBlack), b(64, 64, kBlack);
        Path p;
        p.moveTo(6.0f, 30.0f).cubicTo(20.0f, 2.0f, 44.0f, 58.0f, 58.0f, 30.0f)
         .lineTo(30.0f, 60.0f).close();
        maz::render::fillPath(a, p, kWhite);
        maz::render::fillPath(b, p, kWhite);
        CHECK(a.data() == b.data(), "filling the same path twice gives identical bytes");
    }

    // --- 11. An empty or degenerate path is a no-op, not a crash. ---
    {
        Image img(16, 16, kBlack);
        Path empty;
        maz::render::fillPath(img, empty, kWhite);
        Path dot;
        dot.moveTo(8.0f, 8.0f).close();
        maz::render::fillPath(img, dot, kWhite);
        Path line;
        line.moveTo(2.0f, 2.0f).lineTo(12.0f, 2.0f).close();   // zero-height
        maz::render::fillPath(img, line, kWhite);
        CHECK(ink(img) < 0.001, "empty and degenerate paths draw nothing");
    }

    // --- 12. Alpha is honoured: a half-alpha fill over black is half the ink. ---
    {
        Image img(32, 32, kBlack);
        Path p;
        p.rect(8.0f, 8.0f, 16.0f, 16.0f);
        maz::render::fillPath(img, p, Color{1.0f, 1.0f, 1.0f, 0.5f});
        // The image stores 8 bits per channel, so 0.5 is kept as byte 128 and reads back as 0.50196.
        // Over 256 pixels that is half a unit of ink -- the storage's rounding, not the fill's error.
        CHECK(std::fabs(ink(img) - 128.0) < 1.0, "a half-alpha fill lays down half the ink");
    }

    if (g_fail == 0) {
        std::printf("pathfill: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
