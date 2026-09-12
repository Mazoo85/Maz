// tests/render/imagelineargradient.cpp — verifies the angled linear gradient (render::patterns::linearGradient).
// Ground truths: size honoured; angle 0 runs left→right (each column constant; left==from, right==to); angle π/2
// runs top→bottom (each row constant; top==from, bottom==to, matching verticalGradient); a 45° diagonal puts `from`
// at the top-left corner and `to` at the bottom-right; the midpoint is a ~50% blend; non-positive size is safe.
// Values checked against the gradient definition. Pure CPU, headless.
#include "maz/render/ImagePatterns.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 2.0f / 255.0f; }
static bool colEq(const Color& c, float r, float g, float b) { return near(c.r, r) && near(c.g, g) && near(c.b, b); }

int main() {
    const Color from{0, 0, 0, 1}, to{1, 1, 1, 1};
    const float pi = 3.14159265358979f;

    // --- 1. Angle 0: left -> right; columns constant. ---
    {
        const Image g = patterns::linearGradient(32, 16, 0.0f, from, to);
        CHECK(g.width() == 32 && g.height() == 16, "size honoured");
        CHECK(colEq(g.getPixel(0, 0), 0, 0, 0) && colEq(g.getPixel(31, 0), 1, 1, 1), "angle 0: left=from, right=to");
        bool columnsConstant = true;
        for (int x = 0; x < 32; ++x) {
            const float top = g.getPixel(x, 0).r;
            for (int y = 1; y < 16; ++y)
                if (std::fabs(g.getPixel(x, y).r - top) > 2.0f / 255.0f) columnsConstant = false;
        }
        CHECK(columnsConstant, "angle 0: each column is a constant shade");
    }

    // --- 2. Angle π/2: top -> bottom; rows constant; matches verticalGradient. ---
    {
        const Image g = patterns::linearGradient(16, 32, pi / 2.0f, from, to);
        CHECK(colEq(g.getPixel(0, 0), 0, 0, 0) && colEq(g.getPixel(0, 31), 1, 1, 1), "angle π/2: top=from, bottom=to");
        bool rowsConstant = true;
        for (int y = 0; y < 32; ++y) {
            const float left = g.getPixel(0, y).r;
            for (int x = 1; x < 16; ++x)
                if (std::fabs(g.getPixel(x, y).r - left) > 2.0f / 255.0f) rowsConstant = false;
        }
        CHECK(rowsConstant, "angle π/2: each row is a constant shade");
        const Image v = patterns::verticalGradient(16, 32, from, to);
        bool sameAsVertical = true;
        for (int y = 0; y < 32; ++y)
            if (!near(g.getPixel(0, y).r, v.getPixel(0, y).r)) sameAsVertical = false;
        CHECK(sameAsVertical, "angle π/2 matches verticalGradient");
    }

    // --- 3. 45° diagonal: from at top-left, to at bottom-right. ---
    {
        const Image g = patterns::linearGradient(32, 32, pi / 4.0f, from, to);
        CHECK(colEq(g.getPixel(0, 0), 0, 0, 0), "diagonal: top-left is from");
        CHECK(colEq(g.getPixel(31, 31), 1, 1, 1), "diagonal: bottom-right is to");
        const Color mid = g.getPixel(16, 16);
        CHECK(mid.r > 0.35f && mid.r < 0.65f, "diagonal centre is a ~50% blend");
    }

    // --- 4. Empty-safe. ---
    {
        CHECK(patterns::linearGradient(0, 8, 0.0f, from, to).empty(), "zero width -> empty");
        CHECK(patterns::linearGradient(8, -2, 1.0f, from, to).empty(), "negative height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagelineargradient: OK — sized, angle 0 h-gradient, π/2 == vertical, 45° diagonal, safe.\n");
        return 0;
    }
    std::printf("imagelineargradient: %d failure(s).\n", g_fail);
    return 1;
}
