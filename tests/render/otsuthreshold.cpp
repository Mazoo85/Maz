// tests/render/otsuthreshold.cpp — verifies Otsu automatic thresholding (render OtsuThreshold.hpp).
// Ground truths, deterministic:
//   * a two-value image (half at 50, half at 200) gets a cutoff that separates the two clusters;
//   * a single-valued image returns that value;
//   * a bimodal image with peaks near 60 and 190 gets a threshold in the histogram valley between them;
//   * binarize splits foreground/background exactly at the chosen threshold.
#include "maz/render/OtsuThreshold.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::binarize;
using maz::render::otsuThreshold;

int main() {
    // --- 1. Two spikes: 50 and 200, equal counts. ---
    {
        std::vector<std::uint8_t> img;
        for (int i = 0; i < 100; ++i) img.push_back(50);
        for (int i = 0; i < 100; ++i) img.push_back(200);
        const int t = otsuThreshold(img);
        CHECK(t >= 50 && t < 200, "threshold falls between the two clusters");
        // Binarizing separates them cleanly.
        const std::vector<std::uint8_t> b = binarize(img, t);
        bool ok = true;
        for (int i = 0; i < 100; ++i)
            if (b[static_cast<size_t>(i)] != 0) ok = false; // the 50s
        for (int i = 100; i < 200; ++i)
            if (b[static_cast<size_t>(i)] != 255) ok = false; // the 200s
        CHECK(ok, "binarize puts the dark cluster at 0 and the bright cluster at 255");
    }

    // --- 2. Single-valued image. ---
    {
        std::vector<std::uint8_t> flat(64, 128);
        CHECK(otsuThreshold(flat) == 128, "a single-valued image thresholds at that value");
    }

    // --- 3. Bimodal: two bumps near 60 and 190 -> threshold in the valley. ---
    {
        std::vector<std::uint8_t> img;
        // Bump A around 60 (values 55..65), bump B around 190 (values 185..195).
        for (int v = 55; v <= 65; ++v)
            for (int k = 0; k < 30; ++k) img.push_back(static_cast<std::uint8_t>(v));
        for (int v = 185; v <= 195; ++v)
            for (int k = 0; k < 30; ++k) img.push_back(static_cast<std::uint8_t>(v));
        const int t = otsuThreshold(img);
        CHECK(t > 90 && t < 160, "threshold lands in the valley between the two histogram bumps");
    }

    // --- 4. Empty image. ---
    {
        std::vector<std::uint8_t> empty;
        CHECK(otsuThreshold(empty) == 0, "empty image thresholds at 0");
    }

    if (g_fail == 0) {
        std::printf("otsuthreshold: OK — two-spike separation, single value, bimodal valley, empty.\n");
        return 0;
    }
    std::printf("otsuthreshold: %d failure(s).\n", g_fail);
    return 1;
}
