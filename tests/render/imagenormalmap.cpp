// tests/render/imagenormalmap.cpp — verifies height->normal-map baking (render::heightToNormalMap). Ground truths:
// a flat height image bakes to the classic flat normal (128,128,255) light blue everywhere; a height ramp brightening
// left->right tilts the normal toward -X so red drops below 128; every output normal is unit length (z>0); a bigger
// strength tilts more; an empty input is safe. Pure CPU, headless.
#include "maz/render/ImageNormalMap.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Flat height -> flat normal (0.5,0.5,1.0) = (128,128,255). ---
    {
        Image flat(8, 8, Color{0.5f, 0.5f, 0.5f, 1.0f}); // constant height
        const Image nm = heightToNormalMap(flat, 1.0f);
        const Color c = nm.getPixel(4, 4);
        const float q = 1.0f / 255.0f;
        CHECK(near(c.r, 0.5f, 2 * q) && near(c.g, 0.5f, 2 * q) && near(c.b, 1.0f, 2 * q),
              "a flat heightmap bakes to the flat normal (128,128,255)");
    }

    // --- 2. A left->right brightening ramp tilts the normal toward -X (red < 0.5). ---
    {
        Image ramp(8, 4);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 8; ++x) {
                const float hgt = static_cast<float>(x) / 7.0f; // rises with x
                ramp.setPixel(x, y, Color{hgt, hgt, hgt, 1.0f});
            }
        const Image nm = heightToNormalMap(ramp, 1.0f);
        const Color c = nm.getPixel(4, 2); // interior pixel, clear +X slope
        CHECK(c.r < 0.5f - 1e-3f, "a rightward-rising ramp tilts the normal's X negative (red < 128)");
        CHECK(near(c.g, 0.5f, 2.0f / 255.0f), "no Y slope -> green stays ~128");
    }

    // --- 3. Every baked normal is a unit vector with z>0 (decodes back to length 1). ---
    {
        Image bumpy(6, 6);
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 6; ++x) {
                const float hgt = 0.5f + 0.3f * std::sin(static_cast<float>(x)) * std::cos(static_cast<float>(y));
                bumpy.setPixel(x, y, Color{hgt, hgt, hgt, 1.0f});
            }
        const Image nm = heightToNormalMap(bumpy, 2.0f);
        bool ok = true;
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 6; ++x) {
                const Color c = nm.getPixel(x, y);
                const float nx = c.r * 2 - 1, ny = c.g * 2 - 1, nz = c.b * 2 - 1;
                if (nz <= 0.0f) ok = false;
                if (!near(std::sqrt(nx * nx + ny * ny + nz * nz), 1.0f, 0.02f)) ok = false;
            }
        CHECK(ok, "every baked normal is unit length with z>0");
    }

    // --- 4. Bigger strength tilts more (red drops further from 128 on the same ramp). ---
    {
        Image ramp(8, 4);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 8; ++x) { const float hgt = static_cast<float>(x) / 7.0f; ramp.setPixel(x, y, Color{hgt, hgt, hgt, 1.0f}); }
        const float weak = heightToNormalMap(ramp, 0.5f).getPixel(4, 2).r;
        const float strong = heightToNormalMap(ramp, 4.0f).getPixel(4, 2).r;
        CHECK(strong < weak, "higher strength tilts the normal further (red drops more)");
    }

    // --- 5. Empty input -> empty output. ---
    {
        CHECK(heightToNormalMap(Image{}, 1.0f).empty(), "empty input -> empty output");
    }

    if (g_fail == 0) {
        std::printf("imagenormalmap: OK — flat->flat, ramp tilts -X, unit normals, strength scales, safe.\n");
        return 0;
    }
    std::printf("imagenormalmap: %d failure(s).\n", g_fail);
    return 1;
}
