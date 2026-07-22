// tests/render/sobeledge.cpp — verifies Sobel edge detection (render SobelEdge.hpp).
// Ground truths, hand-computed, deterministic:
//   * a flat (constant) image has zero gradient everywhere;
//   * a vertical step edge (left dark, right bright) gives Gx=+4, Gy=0 at the boundary pixels and ~0
//     away from it; a horizontal step gives Gy=+4, Gx=0;
//   * the gradient magnitude is largest on the edge;
//   * edgeMask flags exactly the high-gradient pixels.
#include "maz/render/SobelEdge.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::EdgeField;
using maz::render::edgeMask;
using maz::render::sobel;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Flat image -> zero gradient. ---
    {
        std::vector<float> flat(5 * 5, 0.7f);
        const EdgeField e = sobel(flat, 5, 5);
        bool allZero = true;
        for (float m : e.mag) {
            if (!near(m, 0.0f)) allZero = false;
        }
        CHECK(allZero, "a constant image has no edges (zero gradient)");
    }

    // --- 2. Vertical step edge: 0 on the left half, 1 on the right. ---
    {
        const int w = 6, h = 4;
        std::vector<float> img(static_cast<size_t>(w * h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                img[static_cast<size_t>(y * w + x)] = x >= w / 2 ? 1.0f : 0.0f; // edge between col 2 and 3
            }
        }
        const EdgeField e = sobel(img, w, h);
        // Interior boundary columns (2 and 3) at an interior row (1) should read Gx=4, Gy=0.
        const size_t iCol2 = static_cast<size_t>(1 * w + 2);
        const size_t iCol3 = static_cast<size_t>(1 * w + 3);
        CHECK(near(e.gx[iCol2], 4.0f) && near(e.gy[iCol2], 0.0f), "vertical edge: Gx=4, Gy=0 at the boundary");
        CHECK(near(e.gx[iCol3], 4.0f), "the other side of the vertical edge also reads Gx=4");
        // A pixel far from the edge (col 0 area is flat 0) reads ~0.
        const size_t iFlat = static_cast<size_t>(1 * w + 0);
        CHECK(near(e.mag[iFlat], 0.0f), "away from the edge the gradient is ~0");
        // The edge magnitude beats the flat region.
        CHECK(e.mag[iCol2] > e.mag[iFlat] + 1.0f, "the edge has the strongest gradient");
    }

    // --- 3. Horizontal step edge: swap of the vertical case. ---
    {
        const int w = 4, h = 6;
        std::vector<float> img(static_cast<size_t>(w * h), 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                img[static_cast<size_t>(y * w + x)] = y >= h / 2 ? 1.0f : 0.0f; // edge between row 2 and 3
            }
        }
        const EdgeField e = sobel(img, w, h);
        const size_t iRow2 = static_cast<size_t>(2 * w + 1);
        CHECK(near(e.gy[iRow2], 4.0f) && near(e.gx[iRow2], 0.0f), "horizontal edge: Gy=4, Gx=0 at the boundary");
    }

    // --- 4. edgeMask thresholds the magnitude. ---
    {
        const int w = 6, h = 4;
        std::vector<float> img(static_cast<size_t>(w * h), 0.0f);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) img[static_cast<size_t>(y * w + x)] = x >= w / 2 ? 1.0f : 0.0f;
        const EdgeField e = sobel(img, w, h);
        const std::vector<unsigned char> mask = edgeMask(e, 2.0f);
        int edgeCount = 0, flatMarked = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t i = static_cast<size_t>(y * w + x);
                if (mask[i]) {
                    ++edgeCount;
                    if (x == 0) ++flatMarked; // leftmost column is flat, must not be an edge
                }
            }
        }
        CHECK(edgeCount > 0, "edgeMask flags the edge pixels");
        CHECK(flatMarked == 0, "edgeMask does not flag the flat region");
    }

    if (g_fail == 0) {
        std::printf("sobeledge: OK — flat=0, vertical edge Gx=4, horizontal edge Gy=4, magnitude peak, "
                    "edgeMask.\n");
        return 0;
    }
    std::printf("sobeledge: %d failure(s).\n", g_fail);
    return 1;
}
