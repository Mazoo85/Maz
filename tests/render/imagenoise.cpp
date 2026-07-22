// tests/render/imagenoise.cpp — verifies the procedural noise texture (render::patterns::noiseTexture). Ground
// truths: size is honoured; every pixel is grey (r==g==b) in [0,1]; the field VARIES across the image (not flat);
// the same seed reproduces the identical texture and a different seed changes it; a non-positive size is safe; the
// output feeds heightToNormalMap (composability). Pure CPU, headless.
#include "maz/render/ImagePatterns.hpp"
#include "maz/render/ImageNormalMap.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

int main() {
    // --- 1. Size honoured; grey pixels in [0,1]; the field varies. ---
    {
        const Image img = patterns::noiseTexture(64, 48, 0.1f, 1234, 4);
        CHECK(img.width() == 64 && img.height() == 48, "noise texture honours the requested size");
        bool grey = true, inRange = true;
        float mn = 2.0f, mx = -1.0f;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const Color c = img.getPixel(x, y);
                if (std::fabs(c.r - c.g) > 2.0f / 255.0f || std::fabs(c.r - c.b) > 2.0f / 255.0f) grey = false;
                if (c.r < -1e-4f || c.r > 1.0f + 1e-4f) inRange = false;
                mn = std::fmin(mn, c.r); mx = std::fmax(mx, c.r);
            }
        CHECK(grey, "noise pixels are grey (r==g==b)");
        CHECK(inRange, "noise values stay in [0,1]");
        CHECK(mx - mn > 0.1f, "the noise field varies across the image (not flat)");
    }

    // --- 2. Deterministic by seed: same seed -> identical, different seed -> different. ---
    {
        const Image a = patterns::noiseTexture(32, 32, 0.12f, 7, 4);
        const Image b = patterns::noiseTexture(32, 32, 0.12f, 7, 4);
        const Image c = patterns::noiseTexture(32, 32, 0.12f, 99, 4);
        bool same = a.data() == b.data();
        bool diff = a.data() != c.data();
        CHECK(same, "same seed reproduces the identical texture");
        CHECK(diff, "a different seed changes the texture");
    }

    // --- 3. Composability: noise -> heightToNormalMap yields a valid (mostly upward) normal map. ---
    {
        const Image nz = patterns::noiseTexture(32, 32, 0.15f, 3, 5);
        const Image nm = heightToNormalMap(nz, 1.0f);
        bool up = true;
        for (int y = 1; y < 31; ++y)
            for (int x = 1; x < 31; ++x)
                if (nm.getPixel(x, y).b < 0.5f) up = false; // z channel (blue) should stay >= 0.5 (normal points out)
        CHECK(up, "a normal map baked from noise points generally outward (blue >= 128)");
    }

    // --- 4. Non-positive size -> empty. ---
    {
        CHECK(patterns::noiseTexture(0, 16).empty(), "zero width -> empty");
        CHECK(patterns::noiseTexture(16, -3).empty(), "negative height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagenoise: OK — sized/grey/in-range, varies, seed-deterministic, feeds normal map, safe.\n");
        return 0;
    }
    std::printf("imagenoise: %d failure(s).\n", g_fail);
    return 1;
}
