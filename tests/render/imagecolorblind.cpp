// tests/render/imagecolorblind.cpp — verifies the colour-blindness simulation (render::simulateColorVision). Ground
// truths: a neutral grey is left unchanged (matrix rows sum to 1); red and green become MORE similar under
// protanopia/deuteranopia (the defining "confusable" property); achromatopsia collapses to luminance grey; alpha is
// preserved; outputs stay in [0,1]; the image overload honours size and is empty-safe. Pure CPU, headless.
#include "maz/render/ImageColorBlind.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b) { return std::fabs(a - b) <= 2.0f / 255.0f; }
static float colDist(const Color& a, const Color& b) {
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}
static bool inRange(const Color& c) {
    return c.r >= -1e-4f && c.r <= 1.0001f && c.g >= -1e-4f && c.g <= 1.0001f && c.b >= -1e-4f && c.b <= 1.0001f;
}

int main() {
    const Color red{1, 0, 0, 1}, green{0, 1, 0, 1};

    // --- 1. Neutral grey is unchanged for the three dichromat types (rows sum to 1). ---
    {
        const Color grey{0.5f, 0.5f, 0.5f, 1.0f};
        for (ColorVision v : {ColorVision::Protanopia, ColorVision::Deuteranopia, ColorVision::Tritanopia}) {
            const Color o = simulateColorVision(grey, v);
            CHECK(near(o.r, 0.5f) && near(o.g, 0.5f) && near(o.b, 0.5f), "a neutral grey is preserved");
        }
    }

    // --- 2. Red and green become MORE similar under red/green deficiencies. ---
    {
        const float baseDist = colDist(red, green); // sqrt(2) ~ 1.414
        for (ColorVision v : {ColorVision::Protanopia, ColorVision::Deuteranopia}) {
            const float simDist = colDist(simulateColorVision(red, v), simulateColorVision(green, v));
            CHECK(simDist < baseDist * 0.5f, "red & green are much closer under red/green colour-blindness");
        }
    }

    // --- 3. Achromatopsia -> luminance grey (r==g==b), and alpha preserved. ---
    {
        const Color o = simulateColorVision(Color{1, 0, 0, 0.4f}, ColorVision::Achromatopsia);
        CHECK(near(o.r, o.g) && near(o.g, o.b), "achromatopsia makes r==g==b (grey)");
        CHECK(near(o.r, luminance(red)), "achromatopsia grey equals the colour's luminance");
        CHECK(near(o.a, 0.4f), "achromatopsia preserves alpha");
    }

    // --- 4. Outputs stay in [0,1] for saturated inputs; alpha preserved through the matrices. ---
    {
        for (ColorVision v : {ColorVision::Protanopia, ColorVision::Deuteranopia, ColorVision::Tritanopia}) {
            const Color o = simulateColorVision(Color{1, 1, 0, 0.7f}, v); // saturated yellow
            CHECK(inRange(o), "simulated colour stays in [0,1]");
            CHECK(near(o.a, 0.7f), "dichromat simulation preserves alpha");
        }
    }

    // --- 5. Image overload: size honoured, per-pixel matches the colour overload, empty-safe. ---
    {
        Image img(4, 3, Color{0.9f, 0.2f, 0.1f, 1.0f});
        img.setPixel(1, 1, Color{0.1f, 0.8f, 0.3f, 1.0f});
        const Image out = simulateColorVision(img, ColorVision::Deuteranopia);
        CHECK(out.width() == 4 && out.height() == 3, "image simulation honours size");
        const Color ref = simulateColorVision(Color{0.1f, 0.8f, 0.3f, 1.0f}, ColorVision::Deuteranopia);
        const Color got = out.getPixel(1, 1);
        CHECK(near(got.r, ref.r) && near(got.g, ref.g) && near(got.b, ref.b),
              "image pixels match the per-colour transform");
        CHECK(simulateColorVision(Image(), ColorVision::Protanopia).empty(), "empty image -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagecolorblind: OK — grey preserved, red/green confusable, achromatopsia grey, alpha, ranged, image.\n");
        return 0;
    }
    std::printf("imagecolorblind: %d failure(s).\n", g_fail);
    return 1;
}
