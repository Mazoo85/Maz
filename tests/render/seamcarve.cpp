// tests/render/seamcarve.cpp — verifies content-aware image resizing (render SeamCarve.hpp).
// Ground truths, deterministic (no <random> from the library, seeded LCG for image content, no clock):
//   * the vertical seam found by DP has minimum total energy — cross-checked against a brute-force
//     enumeration of EVERY 8-connected top-to-bottom seam on small images;
//   * the seam is well-formed: one column per row, in range, consecutive rows differ by <= 1 column;
//   * removeVerticalSeam produces a (w-1) x h image; carveWidth/carveHeight hit the requested size;
//   * a uniform image stays uniform after carving (all seams equal energy);
//   * content-awareness: a high-energy stripe is preserved while flat background is carved away;
//   * degenerate cases (target >= size, width 1) return the image unchanged.
#include "maz/render/SeamCarve.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float unit() { return static_cast<float>(next() % 1000u) / 999.0f; }
};

static Image randomImage(Lcg& rng, int w, int h) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, Color{rng.unit(), rng.unit(), rng.unit(), 1.0f});
    return img;
}

// Sum of energies along a given seam (column per row) using the SAME energy function as the DP.
static double seamEnergy(const Image& img, const std::vector<int>& seam) {
    double e = 0.0;
    for (int y = 0; y < img.height(); ++y)
        e += static_cast<double>(maz::render::seamEnergyAt(img, seam[static_cast<std::size_t>(y)], y));
    return e;
}

// Brute force: the minimum total energy over ALL 8-connected top-to-bottom seams (feasible for small w,h).
static double bruteMinSeam(const Image& img) {
    const int w = img.width(), h = img.height();
    std::vector<int> col(static_cast<std::size_t>(h));
    double best = std::numeric_limits<double>::infinity();
    // Recurse row by row, branching to columns within +-1 of the previous.
    struct Rec {
        static void go(const Image& im, int w_, int h_, int y, std::vector<int>& c, double& best_) {
            if (y == h_) {
                double e = 0.0;
                for (int yy = 0; yy < h_; ++yy)
                    e += static_cast<double>(maz::render::seamEnergyAt(im, c[static_cast<std::size_t>(yy)], yy));
                if (e < best_) best_ = e;
                return;
            }
            const int lo = y == 0 ? 0 : std::max(0, c[static_cast<std::size_t>(y - 1)] - 1);
            const int hi = y == 0 ? w_ - 1 : std::min(w_ - 1, c[static_cast<std::size_t>(y - 1)] + 1);
            for (int x = lo; x <= hi; ++x) {
                c[static_cast<std::size_t>(y)] = x;
                go(im, w_, h_, y + 1, c, best_);
            }
        }
    };
    Rec::go(img, w, h, 0, col, best);
    return best;
}

int main() {
    // --- 1. DP seam is optimal (vs brute force) + well-formed, over many random small images. ---
    {
        Lcg rng{0x5EA3Cu};
        bool optimal = true, wellFormed = true;
        for (int trial = 0; trial < 400 && optimal && wellFormed; ++trial) {
            const int w = 2 + static_cast<int>(rng.next() % 5u); // 2..6
            const int h = 2 + static_cast<int>(rng.next() % 5u); // 2..6
            const Image img = randomImage(rng, w, h);
            const std::vector<int> seam = maz::render::findVerticalSeam(img);
            if (seam.size() != static_cast<std::size_t>(h)) { wellFormed = false; break; }
            for (int y = 0; y < h; ++y) {
                if (seam[static_cast<std::size_t>(y)] < 0 || seam[static_cast<std::size_t>(y)] >= w) wellFormed = false;
                if (y > 0 && std::abs(seam[static_cast<std::size_t>(y)] - seam[static_cast<std::size_t>(y - 1)]) > 1)
                    wellFormed = false;
            }
            const double got = seamEnergy(img, seam);
            const double opt = bruteMinSeam(img);
            if (got > opt + 1e-6) optimal = false; // DP must not exceed the true minimum
        }
        CHECK(optimal, "DP vertical seam has minimum total energy (matches brute force)");
        CHECK(wellFormed, "seam is one-per-row, in range, and 8-connected");
    }

    // --- 2. Removal + carve dimensions. ---
    {
        Lcg rng{0x1234u};
        const Image img = randomImage(rng, 8, 6);
        const Image r = maz::render::removeVerticalSeam(img, maz::render::findVerticalSeam(img));
        CHECK(r.width() == 7 && r.height() == 6, "removeVerticalSeam yields (w-1) x h");
        const Image cw = maz::render::carveWidth(img, 3);
        CHECK(cw.width() == 3 && cw.height() == 6, "carveWidth reaches the target width, height preserved");
        const Image ch = maz::render::carveHeight(img, 2);
        CHECK(ch.width() == 8 && ch.height() == 2, "carveHeight reaches the target height, width preserved");
    }

    // --- 3. Uniform image stays uniform. ---
    {
        Image img(6, 5, Color{0.4f, 0.6f, 0.2f, 1.0f});
        const Image out = maz::render::carveWidth(img, 3);
        bool uniform = out.width() == 3 && out.height() == 5;
        for (int y = 0; y < out.height() && uniform; ++y)
            for (int x = 0; x < out.width(); ++x) {
                const Color c = out.getPixel(x, y);
                if (std::abs(c.r - 0.4f) > 0.02f || std::abs(c.g - 0.6f) > 0.02f || std::abs(c.b - 0.2f) > 0.02f)
                    uniform = false;
            }
        CHECK(uniform, "a flat image stays flat after carving");
    }

    // --- 4. Content-awareness: a high-energy textured band survives; the flat background is carved. ---
    {
        // A 3-column checkerboard band (high gradient at EVERY pixel) at x=4..6 on a flat black field.
        // Seam carving should route seams through the zero-energy background and leave the band intact.
        // (A single 1-px line would NOT survive — its own gradient is zero because both side-neighbours are
        //  background — so the important content must genuinely be high-energy, as a real subject is.)
        const int w = 11, h = 7;
        Image img(w, h, Color{0.0f, 0.0f, 0.0f, 1.0f});
        int brightBefore = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 4; x <= 6; ++x)
                if (((x + y) & 1) == 0) { img.setPixel(x, y, Color{1.0f, 1.0f, 1.0f, 1.0f}); ++brightBefore; }
        const Image out = maz::render::carveWidth(img, 5); // remove 6 columns, all from the flat background
        CHECK(out.width() == 5 && out.height() == 7, "band test carves to target size");
        int brightAfter = 0;
        for (int y = 0; y < out.height(); ++y)
            for (int x = 0; x < out.width(); ++x) {
                const Color c = out.getPixel(x, y);
                if (c.r > 0.5f && c.g > 0.5f && c.b > 0.5f) ++brightAfter;
            }
        CHECK(brightBefore > 0 && brightAfter == brightBefore,
              "the high-energy textured band is preserved; only flat background is carved");
    }

    // --- 5. Degenerate cases. ---
    {
        Lcg rng{0x77u};
        const Image img = randomImage(rng, 5, 4);
        CHECK(maz::render::carveWidth(img, 5).width() == 5, "target == width -> unchanged width");
        CHECK(maz::render::carveWidth(img, 9).width() == 5, "target > width -> unchanged");
        Image col(1, 4, Color{0.3f, 0.3f, 0.3f, 1.0f});
        CHECK(maz::render::carveWidth(col, 1).width() == 1, "width-1 image is left alone");
        CHECK(maz::render::findVerticalSeam(col).empty(), "no vertical seam in a 1-wide image");
    }

    if (g_fail == 0) {
        std::printf("seamcarve: OK — DP optimality vs brute force, seam shape, carve dims, uniform, stripe, degenerate.\n");
        return 0;
    }
    std::printf("seamcarve: %d failure(s).\n", g_fail);
    return 1;
}
