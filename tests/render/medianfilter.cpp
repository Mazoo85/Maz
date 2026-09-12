// tests/render/medianfilter.cpp — verifies the median filter (render MedianFilter.hpp).
// Ground truths, hand-computed, deterministic:
//   * a flat image is unchanged;
//   * a single bright speckle in a flat field is removed (the pixel takes the surrounding value);
//   * a sharp vertical edge stays sharp (median does NOT blur it, unlike a box blur);
//   * a hand-computed 3x3 window median matches.
#include "maz/render/MedianFilter.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::medianFilter;

int main() {
    // --- 1. Flat image is unchanged. ---
    {
        std::vector<std::uint8_t> flat(5 * 5, 120);
        const std::vector<std::uint8_t> out = medianFilter(flat, 5, 5);
        bool same = true;
        for (std::uint8_t v : out)
            if (v != 120) same = false;
        CHECK(same, "a flat image passes through unchanged");
    }

    // --- 2. Salt speckle removed. ---
    {
        const int w = 5, h = 5;
        std::vector<std::uint8_t> img(static_cast<size_t>(w * h), 100);
        img[static_cast<size_t>(2 * w + 2)] = 255; // a lone bright speckle at the centre
        const std::vector<std::uint8_t> out = medianFilter(img, w, h);
        CHECK(out[static_cast<size_t>(2 * w + 2)] == 100, "a lone speckle is replaced by the surrounding value");
        // And a pepper speckle too.
        std::vector<std::uint8_t> img2(static_cast<size_t>(w * h), 100);
        img2[static_cast<size_t>(2 * w + 2)] = 0;
        const std::vector<std::uint8_t> out2 = medianFilter(img2, w, h);
        CHECK(out2[static_cast<size_t>(2 * w + 2)] == 100, "a dark speckle is removed too");
    }

    // --- 3. Sharp edge preserved. ---
    {
        const int w = 6, h = 4;
        std::vector<std::uint8_t> img(static_cast<size_t>(w * h), 0);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) img[static_cast<size_t>(y * w + x)] = x >= 3 ? 255 : 0;
        const std::vector<std::uint8_t> out = medianFilter(img, w, h);
        // A pixel just left of the edge (col 2) stays 0; just right (col 3) stays 255 — no smearing.
        CHECK(out[static_cast<size_t>(1 * w + 2)] == 0, "left of a sharp edge stays dark (no blur)");
        CHECK(out[static_cast<size_t>(1 * w + 3)] == 255, "right of a sharp edge stays bright (no blur)");
    }

    // --- 4. Hand-computed 3x3 window median. ---
    {
        // 3x3 image; the centre's window is all nine values. Median of {10,20,30,40,50,60,70,80,90}=50.
        const std::vector<std::uint8_t> img = {10, 20, 30, 40, 50, 60, 70, 80, 90};
        const std::vector<std::uint8_t> out = medianFilter(img, 3, 3);
        CHECK(out[4] == 50, "centre pixel becomes the median (50) of its 3x3 window");
    }

    if (g_fail == 0) {
        std::printf("medianfilter: OK — flat unchanged, salt/pepper removed, edge preserved, window "
                    "median.\n");
        return 0;
    }
    std::printf("medianfilter: %d failure(s).\n", g_fail);
    return 1;
}
