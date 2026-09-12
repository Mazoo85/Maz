// tests/render/imagecellular.cpp — verifies the procedural cellular ("Worley") texture
// (render::patterns::cellularTexture). Ground truths: size is honoured; every pixel is grey (r==g==b) in [0,1]; the
// field VARIES across the image (not flat); the same seed reproduces the identical texture and a different seed
// changes it; the two modes (Cells vs Cracks) produce visibly different fields and each spans dark-to-bright; the
// Cracks field has thin dark boundary lines (some near-black pixels); a non-positive size is safe; the output feeds
// heightToNormalMap (composability). Pure CPU, headless.
#include "maz/render/ImagePatterns.hpp"
#include "maz/render/ImageNormalMap.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static int countBelow(const Image& img, float thresh) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.getPixel(x, y).r < thresh) ++n;
    return n;
}

int main() {
    // --- 1. Size honoured; grey pixels in [0,1]; the field varies. ---
    {
        const Image img = patterns::cellularTexture(64, 48, 0.1f, 1234, patterns::Cellular::Cells);
        CHECK(img.width() == 64 && img.height() == 48, "cellular texture honours the requested size");
        bool grey = true, inRange = true;
        float mn = 2.0f, mx = -1.0f;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const Color c = img.getPixel(x, y);
                if (std::fabs(c.r - c.g) > 2.0f / 255.0f || std::fabs(c.r - c.b) > 2.0f / 255.0f) grey = false;
                if (c.r < -1e-4f || c.r > 1.0f + 1e-4f) inRange = false;
                mn = std::fmin(mn, c.r); mx = std::fmax(mx, c.r);
            }
        CHECK(grey, "cellular pixels are grey (r==g==b)");
        CHECK(inRange, "cellular values stay in [0,1]");
        CHECK(mx - mn > 0.1f, "the cellular field varies across the image (not flat)");
    }

    // --- 2. Deterministic by seed: same seed -> identical, different seed -> different. ---
    {
        const Image a = patterns::cellularTexture(32, 32, 0.12f, 7);
        const Image b = patterns::cellularTexture(32, 32, 0.12f, 7);
        const Image c = patterns::cellularTexture(32, 32, 0.12f, 99);
        CHECK(a.data() == b.data(), "same seed reproduces the identical texture");
        CHECK(a.data() != c.data(), "a different seed changes the texture");
    }

    // --- 3. The two modes differ; each spans dark-to-bright; Cracks has thin near-black boundary lines. ---
    {
        const Image cells  = patterns::cellularTexture(48, 48, 0.1f, 5, patterns::Cellular::Cells);
        const Image cracks = patterns::cellularTexture(48, 48, 0.1f, 5, patterns::Cellular::Cracks);
        CHECK(cells.data() != cracks.data(), "Cells and Cracks modes differ");
        CHECK(countBelow(cracks, 0.1f) > 0, "Cracks has near-black boundary-line pixels");
        CHECK(countBelow(cracks, 0.9f) < 48 * 48, "Cracks also has bright interior pixels (spans the range)");
    }

    // --- 4. Composability: cellular -> heightToNormalMap yields a valid (mostly upward) normal map. ---
    {
        const Image cz = patterns::cellularTexture(32, 32, 0.15f, 3);
        const Image nm = heightToNormalMap(cz, 1.0f);
        bool up = true;
        for (int y = 1; y < 31; ++y)
            for (int x = 1; x < 31; ++x)
                if (nm.getPixel(x, y).b < 0.5f) up = false; // z channel (blue) should stay >= 0.5 (normal points out)
        CHECK(up, "a normal map baked from cellular noise points generally outward (blue >= 128)");
    }

    // --- 5. Non-positive size -> empty. ---
    {
        CHECK(patterns::cellularTexture(0, 16).empty(), "zero width -> empty");
        CHECK(patterns::cellularTexture(16, -3).empty(), "negative height -> empty");
    }

    if (g_fail == 0) {
        std::printf("imagecellular: OK — sized/grey/in-range, varies, seed-deterministic, modes differ, safe.\n");
        return 0;
    }
    std::printf("imagecellular: %d failure(s).\n", g_fail);
    return 1;
}
