// tests/render/colorhsl.cpp — verifies classic HSL <-> RGB conversion (render::fromHsl / toHsl).
// Ground truths: the canonical CSS anchor colours map to their documented HSL (red = h0 s1 l0.5, white =
// l1, black = l0, mid-grey = s0 l0.5); HSL differs from HSV as designed (pure red is l=0.5, not v-style
// l=1); a full RGB->HSL->RGB round-trip is the identity across a colour grid; hue wraps; saturation 0 is a
// pure grey at the lightness; alpha passes through; and lightness runs black->colour->white symmetrically.
// Values checked against the W3C HSL algorithm. Pure CPU, deterministic.
#include "maz/render/ColorOps.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static bool colEq(const Color& c, float r, float g, float b, float eps = 1e-4f) {
    return near(c.r, r, eps) && near(c.g, g, eps) && near(c.b, b, eps);
}

int main() {
    // --- 1. Canonical anchors: HSL of the primaries and greys. ---
    {
        const Hsl red = toHsl(Color{1, 0, 0, 1});
        CHECK(near(red.h, 0.0f) && near(red.s, 1.0f) && near(red.l, 0.5f), "red -> h0 s1 l0.5");
        const Hsl white = toHsl(Color{1, 1, 1, 1});
        CHECK(near(white.l, 1.0f) && near(white.s, 0.0f), "white -> l1 s0");
        const Hsl black = toHsl(Color{0, 0, 0, 1});
        CHECK(near(black.l, 0.0f) && near(black.s, 0.0f), "black -> l0 s0");
        const Hsl grey = toHsl(Color{0.5f, 0.5f, 0.5f, 1});
        CHECK(near(grey.s, 0.0f) && near(grey.l, 0.5f), "mid-grey -> s0 l0.5");
        // Green at 120 degrees (h=1/3), blue at 240 (h=2/3).
        CHECK(near(toHsl(Color{0, 1, 0, 1}).h, 1.0f / 3.0f), "green -> h=1/3");
        CHECK(near(toHsl(Color{0, 0, 1, 1}).h, 2.0f / 3.0f), "blue -> h=2/3");
    }

    // --- 2. fromHsl reconstructs the anchors. ---
    {
        CHECK(colEq(fromHsl(0.0f, 1.0f, 0.5f), 1, 0, 0), "h0 s1 l0.5 -> red");
        CHECK(colEq(fromHsl(1.0f / 3.0f, 1.0f, 0.5f), 0, 1, 0), "h1/3 -> green");
        CHECK(colEq(fromHsl(2.0f / 3.0f, 1.0f, 0.5f), 0, 0, 1), "h2/3 -> blue");
        CHECK(colEq(fromHsl(0.0f, 0.0f, 1.0f), 1, 1, 1), "l1 -> white");
        CHECK(colEq(fromHsl(0.0f, 0.0f, 0.0f), 0, 0, 0), "l0 -> black");
    }

    // --- 3. HSL genuinely differs from HSV (the defining distinction). ---
    {
        const Hsl hsl = toHsl(Color{1, 0, 0, 1});
        const Hsv hsv = toHsv(Color{1, 0, 0, 1});
        CHECK(near(hsl.l, 0.5f) && near(hsv.v, 1.0f), "pure red: HSL l=0.5 but HSV v=1.0 (not the same model)");
    }

    // --- 4. Lightness is symmetric: black -> full colour -> white through l. ---
    {
        // A fixed hue/sat swept in lightness: l=0 black, l=0.5 the vivid colour, l=1 white.
        CHECK(colEq(fromHsl(0.0f, 1.0f, 0.0f), 0, 0, 0), "l=0 is black regardless of hue/sat");
        CHECK(colEq(fromHsl(0.0f, 1.0f, 1.0f), 1, 1, 1), "l=1 is white regardless of hue/sat");
        const Color vivid = fromHsl(0.0f, 1.0f, 0.5f);
        CHECK(vivid.r > vivid.g && vivid.g <= 1e-4f, "l=0.5 is the vivid hue");
    }

    // --- 5. Round-trip identity across a colour grid. ---
    {
        for (int r = 0; r <= 4; ++r)
            for (int g = 0; g <= 4; ++g)
                for (int b = 0; b <= 4; ++b) {
                    const Color c{static_cast<float>(r) / 4.0f, static_cast<float>(g) / 4.0f,
                                  static_cast<float>(b) / 4.0f, 0.7f};
                    const Color rt = fromHsl(toHsl(c));
                    CHECK(colEq(rt, c.r, c.g, c.b, 1e-4f), "RGB->HSL->RGB round-trips");
                    CHECK(near(rt.a, 0.7f), "alpha preserved through round-trip");
                }
    }

    // --- 6. Hue wraps and saturation 0 yields a pure grey. ---
    {
        CHECK(colEq(fromHsl(1.0f, 1.0f, 0.5f), 1, 0, 0), "hue 1.0 wraps to hue 0 (red)");
        CHECK(colEq(fromHsl(-1.0f / 3.0f, 1.0f, 0.5f), 0, 0, 1), "hue -1/3 wraps to 2/3 (blue)");
        const Color grey = fromHsl(0.42f, 0.0f, 0.3f);
        CHECK(colEq(grey, 0.3f, 0.3f, 0.3f), "s=0 -> grey at the lightness, hue ignored");
    }

    if (g_fail == 0) {
        std::printf("colorhsl: OK — anchors, reconstruct, differs-from-HSV, symmetric-L, round-trip, wrap/grey.\n");
        return 0;
    }
    std::printf("colorhsl: %d failure(s).\n", g_fail);
    return 1;
}
