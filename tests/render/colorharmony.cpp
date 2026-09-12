// tests/render/colorharmony.cpp — verifies the colour-harmony generators (render::rotateHue / complementary /
// analogous / triadic / splitComplementary / tetradic / monochromatic). Ground truths: hue rotations land at the
// right wheel positions (complement +0.5, triad +1/3/+2/3, tetrad quarters), saturation & value are preserved, a
// full turn is identity, monochromatic keeps hue/sat and steps value; set sizes are correct and count<1 is empty.
// Hue positions checked against HSV. Pure CPU, headless.
#include "maz/render/ColorHarmony.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static float frac(float x) { return x - std::floor(x); }
// Shortest distance between two hues on the [0,1) wheel.
static float hueDist(float a, float b) {
    const float d = frac(a - b);
    return d < 0.5f ? d : 1.0f - d;
}

int main() {
    // A saturated base so hue is well-defined: orange-ish.
    const Color base = fromHsv(0.08f, 0.9f, 0.85f);
    const Hsv bh = toHsv(base);

    // --- 1. rotateHue lands at the target hue and preserves S/V; a full turn is identity. ---
    {
        const Hsv r = toHsv(rotateHue(base, 0.25f));
        CHECK(hueDist(r.h, bh.h + 0.25f) < 0.005f, "rotateHue(+0.25) shifts hue by a quarter turn");
        CHECK(std::fabs(r.s - bh.s) < 0.01f && std::fabs(r.v - bh.v) < 0.01f, "rotateHue preserves saturation & value");
        const Color full = rotateHue(base, 1.0f);
        CHECK(std::fabs(full.r - base.r) < 2.0f / 255.0f && std::fabs(full.g - base.g) < 2.0f / 255.0f &&
              std::fabs(full.b - base.b) < 2.0f / 255.0f, "a full turn is the identity");
    }

    // --- 2. Complementary: 2 colours, second is opposite (+0.5). ---
    {
        const std::vector<Color> c = complementary(base);
        CHECK(c.size() == 2, "complementary has 2 colours");
        CHECK(hueDist(toHsv(c[1]).h, bh.h + 0.5f) < 0.005f, "complement is the opposite hue");
    }

    // --- 3. Triadic: 3 colours at +0, +1/3, +2/3. ---
    {
        const std::vector<Color> t = triadic(base);
        CHECK(t.size() == 3, "triadic has 3 colours");
        CHECK(hueDist(toHsv(t[0]).h, bh.h) < 0.005f, "triad[0] is the base hue");
        CHECK(hueDist(toHsv(t[1]).h, bh.h + 1.0f / 3.0f) < 0.005f, "triad[1] is +120 degrees");
        CHECK(hueDist(toHsv(t[2]).h, bh.h + 2.0f / 3.0f) < 0.005f, "triad[2] is +240 degrees");
    }

    // --- 4. Analogous + split-complementary set sizes and neighbour spread. ---
    {
        const std::vector<Color> a = analogous(base, 1.0f / 12.0f);
        CHECK(a.size() == 3, "analogous has 3 colours");
        CHECK(hueDist(toHsv(a[0]).h, bh.h - 1.0f / 12.0f) < 0.005f, "analogous[0] is -30 degrees");
        CHECK(hueDist(toHsv(a[2]).h, bh.h + 1.0f / 12.0f) < 0.005f, "analogous[2] is +30 degrees");
        const std::vector<Color> sc = splitComplementary(base, 1.0f / 12.0f);
        CHECK(sc.size() == 3, "split-complementary has 3 colours");
        CHECK(hueDist(toHsv(sc[1]).h, bh.h + 0.5f - 1.0f / 12.0f) < 0.005f, "split[1] flanks the complement");
        CHECK(hueDist(toHsv(sc[2]).h, bh.h + 0.5f + 1.0f / 12.0f) < 0.005f, "split[2] flanks the complement");
    }

    // --- 5. Tetradic: 4 colours on the square. ---
    {
        const std::vector<Color> q = tetradic(base);
        CHECK(q.size() == 4, "tetradic has 4 colours");
        CHECK(hueDist(toHsv(q[1]).h, bh.h + 0.25f) < 0.005f && hueDist(toHsv(q[2]).h, bh.h + 0.5f) < 0.005f &&
              hueDist(toHsv(q[3]).h, bh.h + 0.75f) < 0.005f, "tetrad hues are quarter turns");
    }

    // --- 6. Monochromatic: same hue & saturation, value steps up to the base value. ---
    {
        const std::vector<Color> m = monochromatic(base, 5);
        CHECK(m.size() == 5, "monochromatic honours the count");
        bool sameHueSat = true, ascending = true;
        float prevV = -1.0f;
        for (const Color& c : m) {
            const Hsv h = toHsv(c);
            if (hueDist(h.h, bh.h) > 0.01f || std::fabs(h.s - bh.s) > 0.02f) sameHueSat = false;
            if (h.v <= prevV) ascending = false;
            prevV = h.v;
        }
        CHECK(sameHueSat, "monochromatic keeps hue & saturation");
        CHECK(ascending, "monochromatic value increases toward the base");
        CHECK(std::fabs(toHsv(m.back()).v - bh.v) < 0.01f, "the brightest shade equals the base value");
        CHECK(monochromatic(base, 0).empty(), "count < 1 -> empty");
    }

    if (g_fail == 0) {
        std::printf("colorharmony: OK — rotateHue, complement/triad/analog/split/tetrad positions, monochrome ramp.\n");
        return 0;
    }
    std::printf("colorharmony: %d failure(s).\n", g_fail);
    return 1;
}
