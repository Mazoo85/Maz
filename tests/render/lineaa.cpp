// tests/render/lineaa.cpp — verifies Xiaolin Wu anti-aliased line drawing (render LineAA.hpp).
// Ground truths, deterministic (fixed geometry + seeded LCG, no <random>, no clock):
//   * a perfectly horizontal/vertical line lights exactly one row/column at full coverage;
//   * a 45-degree diagonal lights one full-coverage pixel per step;
//   * the anti-aliasing energy invariant: for a shallow non-integer-slope line, the two straddling pixels
//     in each interior column have coverage summing to ~1;
//   * total ink (summed coverage) approximates the line's pixel span;
//   * drawing A->B equals drawing B->A (order independence).
#include "maz/render/LineAA.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Coverage read back from a white-on-black draw = the red channel as 0..1.
static float cov(const Image& img, int x, int y) { return img.getPixel(x, y).r; }

int main() {
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};
    const Color black{0.0f, 0.0f, 0.0f, 1.0f};

    // --- 1. Horizontal line: row y=10 full, neighbours dark. ---
    {
        Image img(32, 32, black);
        maz::render::drawLineAA(img, 4.0f, 10.0f, 27.0f, 10.0f, white);
        bool full = true, clean = true;
        for (int x = 5; x <= 26; ++x) {
            if (cov(img, x, 10) < 0.98f) full = false;
            if (cov(img, x, 9) > 0.02f || cov(img, x, 11) > 0.02f) clean = false;
        }
        CHECK(full, "horizontal line is full coverage along its row");
        CHECK(clean, "horizontal line leaves neighbouring rows dark");
    }

    // --- 2. Vertical line: column x=15 full. ---
    {
        Image img(32, 32, black);
        maz::render::drawLineAA(img, 15.0f, 3.0f, 15.0f, 28.0f, white);
        bool full = true, clean = true;
        for (int y = 4; y <= 27; ++y) {
            if (cov(img, 15, y) < 0.98f) full = false;
            if (cov(img, 14, y) > 0.02f || cov(img, 16, y) > 0.02f) clean = false;
        }
        CHECK(full && clean, "vertical line is full coverage along its column, neighbours dark");
    }

    // --- 3. 45-degree diagonal: one full pixel per step. ---
    {
        Image img(32, 32, black);
        maz::render::drawLineAA(img, 2.0f, 2.0f, 25.0f, 25.0f, white);
        bool ok = true;
        for (int i = 4; i <= 23; ++i)
            if (cov(img, i, i) < 0.95f) ok = false;
        CHECK(ok, "45-degree diagonal lights the diagonal pixels at full coverage");
    }

    // --- 4. Anti-aliasing energy invariant on a shallow line. ---
    {
        Image img(64, 32, black);
        // slope 1/3 -> genuinely fractional coverage split each column.
        maz::render::drawLineAA(img, 4.0f, 8.3f, 58.0f, 26.3f, white);
        bool energyOk = true;
        int checked = 0;
        for (int x = 8; x <= 54; ++x) {
            float col = 0.0f;
            for (int y = 0; y < 32; ++y) col += cov(img, x, y);
            if (col < 0.90f || col > 1.10f) energyOk = false; // ~1 unit of ink per column
            ++checked;
        }
        CHECK(checked > 40 && energyOk, "each interior column carries ~1 unit of coverage (AA energy conserved)");
    }

    // --- 5. Total ink approximates the pixel span; order independence. ---
    {
        Image a(48, 48, black), b(48, 48, black);
        maz::render::drawLineAA(a, 5.0f, 6.0f, 40.0f, 30.0f, white);
        maz::render::drawLineAA(b, 40.0f, 30.0f, 5.0f, 6.0f, white);
        float sa = 0.0f;
        bool same = true;
        for (int y = 0; y < 48; ++y)
            for (int x = 0; x < 48; ++x) {
                sa += cov(a, x, y);
                if (std::fabs(cov(a, x, y) - cov(b, x, y)) > 0.02f) same = false;
            }
        CHECK(same, "drawing A->B equals drawing B->A");
        // Major-axis span is dx = 35 -> ~36 columns each ~1 unit of ink.
        CHECK(std::fabs(sa - 36.0f) < 3.0f, "total ink approximates the line's pixel span");
    }

    // --- 6. Determinism. ---
    {
        Image a(24, 24, black), b(24, 24, black);
        maz::render::drawLineAA(a, 1.5f, 2.5f, 20.5f, 15.5f, white);
        maz::render::drawLineAA(b, 1.5f, 2.5f, 20.5f, 15.5f, white);
        CHECK(a.data() == b.data(), "identical draws produce identical images");
    }

    if (g_fail == 0) {
        std::printf("lineaa: OK — H/V/diagonal exactness, AA energy, ink span, order independence, determinism.\n");
        return 0;
    }
    std::printf("lineaa: %d failure(s).\n", g_fail);
    return 1;
}
