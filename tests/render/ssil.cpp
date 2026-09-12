// tests/render/ssil.cpp — verifies the screen-space indirect-light gather kernel (render::ssilGather).
// Ground truths, all pure float math, deterministic:
//   * COLOR BLEED: a floor pixel flanked by a red wall and a white wall receives indirect light whose
//     red channel exceeds its green/blue (the red wall tints the floor) — the defining SSIL behaviour;
//   * HEMISPHERE: a neighbour behind the surface (negative dot with the normal) contributes nothing;
//   * RANGE: a neighbour beyond worldRadius contributes nothing;
//   * INTENSITY scales the result linearly, and zero intensity yields no indirect light;
//   * a coplanar same-normal neighbourhood self-bounces zero (a flat wall doesn't light itself);
//   * determinism (same inputs -> same output).
#include "maz/render/ScreenSpaceIndirectLight.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::SsilColor;
using maz::render::SsilParams;
using maz::render::SsilSample;
using maz::render::ssilGather;

// A 3x1 "concave corner": red wall (left), floor (center, measured), white wall (right). The walls are
// raised (pz=0.6) so their direction from the floor has a positive up-component -> in the hemisphere.
static std::vector<SsilSample> corner() {
    SsilSample redWall;
    redWall.px = -1.0f; redWall.py = 0.0f; redWall.pz = 0.6f;
    redWall.nx = 1.0f;  redWall.ny = 0.0f; redWall.nz = 0.0f; // faces right (irrelevant for gather)
    redWall.r = 1.0f; redWall.g = 0.0f; redWall.b = 0.0f;

    SsilSample floorPx;
    floorPx.px = 0.0f; floorPx.py = 0.0f; floorPx.pz = 0.0f;
    floorPx.nx = 0.0f; floorPx.ny = 0.0f; floorPx.nz = 1.0f; // faces up
    floorPx.r = 0.0f; floorPx.g = 0.0f; floorPx.b = 0.0f;

    SsilSample whiteWall;
    whiteWall.px = 1.0f; whiteWall.py = 0.0f; whiteWall.pz = 0.6f;
    whiteWall.nx = -1.0f; whiteWall.ny = 0.0f; whiteWall.nz = 0.0f;
    whiteWall.r = 1.0f; whiteWall.g = 1.0f; whiteWall.b = 1.0f;

    return {redWall, floorPx, whiteWall};
}

int main() {
    // --- 1. Color bleed: the floor gathers red from the red wall. ---
    {
        SsilParams p;
        p.radiusPx = 1;
        p.worldRadius = 3.0f;
        p.bias = 0.0f;
        p.intensity = 1.0f;
        const auto out = ssilGather(3, 1, corner(), p);
        const SsilColor& floorIndirect = out[1];
        CHECK(floorIndirect.r > 0.0f, "floor receives indirect light");
        CHECK(floorIndirect.r > floorIndirect.g, "red wall tints the floor red (r > g)");
        CHECK(floorIndirect.g > 0.0f, "white wall still contributes some green");
        // Symmetry: both walls equidistant, so g and b (from the white wall only) are equal.
        CHECK(std::fabs(floorIndirect.g - floorIndirect.b) < 1e-6f, "green == blue (only white wall)");
    }

    // --- 2. Intensity scales linearly; zero intensity -> nothing. ---
    {
        SsilParams p;
        p.radiusPx = 1; p.worldRadius = 3.0f;
        p.intensity = 1.0f;
        const float r1 = ssilGather(3, 1, corner(), p)[1].r;
        p.intensity = 2.0f;
        const float r2 = ssilGather(3, 1, corner(), p)[1].r;
        CHECK(std::fabs(r2 - 2.0f * r1) < 1e-6f, "intensity scales indirect linearly");
        p.intensity = 0.0f;
        const SsilColor z = ssilGather(3, 1, corner(), p)[1];
        CHECK(z.r == 0.0f && z.g == 0.0f && z.b == 0.0f, "zero intensity -> no indirect");
    }

    // --- 3. Range rejection: shrink the radius below the wall distance. ---
    {
        SsilParams p;
        p.radiusPx = 1;
        p.worldRadius = 0.5f; // walls are ~1.17 units away
        const SsilColor c = ssilGather(3, 1, corner(), p)[1];
        CHECK(c.r == 0.0f && c.g == 0.0f && c.b == 0.0f, "neighbours beyond radius don't contribute");
    }

    // --- 4. Hemisphere rejection: a neighbour behind the surface contributes nothing. ---
    {
        // Floor faces up (+z); put a bright neighbour BELOW it (pz negative) -> behind the surface.
        SsilSample floorPx;
        floorPx.nz = 1.0f; // faces up, at origin, black
        SsilSample below;
        below.px = 1.0f; below.pz = -0.6f; // below the floor plane
        below.r = 1.0f; below.g = 1.0f; below.b = 1.0f;
        const std::vector<SsilSample> img = {floorPx, below};
        SsilParams p;
        p.radiusPx = 1; p.worldRadius = 3.0f;
        const SsilColor c = ssilGather(2, 1, img, p)[0];
        CHECK(c.r == 0.0f && c.g == 0.0f && c.b == 0.0f, "neighbour behind the surface is ignored");
    }

    // --- 5. Coplanar same-normal wall self-bounces zero. ---
    {
        // Two up-facing coplanar pixels: direction between them is horizontal, perpendicular to the
        // normal, so dot(normal, dir) == 0 -> no contribution.
        SsilSample a;
        a.px = 0.0f; a.nz = 1.0f; a.r = 1.0f; a.g = 1.0f; a.b = 1.0f;
        SsilSample b;
        b.px = 1.0f; b.nz = 1.0f; b.r = 1.0f; b.g = 1.0f; b.b = 1.0f;
        SsilParams p;
        p.radiusPx = 1; p.worldRadius = 3.0f;
        const SsilColor c = ssilGather(2, 1, {a, b}, p)[0];
        CHECK(c.r == 0.0f && c.g == 0.0f && c.b == 0.0f, "coplanar flat surface doesn't light itself");
    }

    // --- 6. Determinism. ---
    {
        SsilParams p;
        p.radiusPx = 1; p.worldRadius = 3.0f;
        const auto a = ssilGather(3, 1, corner(), p);
        const auto b = ssilGather(3, 1, corner(), p);
        CHECK(a.size() == b.size() && a[1].r == b[1].r && a[1].g == b[1].g,
              "deterministic output");
    }

    if (g_fail == 0) {
        std::printf("ssil: OK — color bleed, intensity, range, hemisphere, coplanar-zero, determinism.\n");
        return 0;
    }
    std::printf("ssil: %d failure(s).\n", g_fail);
    return 1;
}
