// tests/render/contrastratio.cpp — verifies the WCAG contrast helpers (render::contrastRatio / relativeLuminance /
// passesAA / passesAAA / bestTextColor). Ground truths: black-on-white is the maximum 21:1; identical colours are
// 1:1; the ratio is symmetric; relative luminance is 0 for black and 1 for white; the AA/AAA thresholds gate
// correctly (a known 4.54:1 pair passes AA-normal but fails AAA-normal); bestTextColor picks the readable option.
// Values checked against the published WCAG formula. Pure CPU, headless.
#include "maz/render/ContrastRatio.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    const Color black{0, 0, 0, 1}, white{1, 1, 1, 1};

    // --- 1. Relative luminance endpoints. ---
    {
        CHECK(near(relativeLuminance(black), 0.0f, 1e-4f), "black has luminance 0");
        CHECK(near(relativeLuminance(white), 1.0f, 1e-4f), "white has luminance 1");
    }

    // --- 2. Extremes and identity of the ratio. ---
    {
        CHECK(near(contrastRatio(black, white), 21.0f, 1e-3f), "black on white is the maximum 21:1");
        CHECK(near(contrastRatio(white, white), 1.0f, 1e-4f), "identical colours are 1:1");
        CHECK(near(contrastRatio(black, white), contrastRatio(white, black), 1e-5f), "ratio is symmetric");
    }

    // --- 3. A known mid pair: #767676 grey on white ~ 4.54:1 (the canonical AA-normal boundary colour). ---
    {
        const float g = 0x76 / 255.0f;
        const Color grey{g, g, g, 1};
        const float r = contrastRatio(grey, white);
        CHECK(near(r, 4.54f, 0.05f), "#767676 on white is ~4.54:1 (WCAG reference value)");
        CHECK(passesAA(grey, white, false), "4.54:1 passes AA normal (>=4.5)");
        CHECK(!passesAAA(grey, white, false), "4.54:1 fails AAA normal (<7.0)");
        CHECK(passesAA(grey, white, true), "4.54:1 passes AA large (>=3.0)");
    }

    // --- 4. Threshold gating at the boundaries. ---
    {
        // black on white (21) passes everything; white on white (1) passes nothing.
        CHECK(passesAAA(black, white, false) && passesAA(black, white, false), "21:1 passes AA & AAA");
        CHECK(!passesAA(white, white, false) && !passesAA(white, white, true), "1:1 fails AA at both sizes");
        // A pair around 3.5:1 passes AA-large but not AA-normal.
        const float m = 0x94 / 255.0f; // ~ #949494 -> ~3.4:1 on white
        const Color mid{m, m, m, 1};
        CHECK(passesAA(mid, white, true), "~3.4:1 passes AA large (>=3.0)");
        CHECK(!passesAA(mid, white, false), "~3.4:1 fails AA normal (<4.5)");
    }

    // --- 5. bestTextColor picks the more legible of black/white. ---
    {
        CHECK(bestTextColor(white).r < 0.5f, "on a white background, black text is chosen");
        CHECK(bestTextColor(black).r > 0.5f, "on a black background, white text is chosen");
        // On a bright yellow, black wins (yellow is very light).
        CHECK(bestTextColor(Color{1, 1, 0, 1}).r < 0.5f, "on bright yellow, black text is chosen");
        // On a dark navy, white wins.
        CHECK(bestTextColor(Color{0.05f, 0.05f, 0.3f, 1}).r > 0.5f, "on dark navy, white text is chosen");
    }

    if (g_fail == 0) {
        std::printf("contrastratio: OK — endpoints, 21:1/1:1, symmetric, WCAG ref pair, AA/AAA gating, bestTextColor.\n");
        return 0;
    }
    std::printf("contrastratio: %d failure(s).\n", g_fail);
    return 1;
}
