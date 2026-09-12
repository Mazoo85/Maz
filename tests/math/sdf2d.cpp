// tests/math/sdf2d.cpp — verifies the analytic 2D signed distance functions (math Sdf2D.hpp).
// Ground truths, deterministic (fixed sample grids, no <random>, no clock):
//   * EIKONAL (airtight, the universal SDF property): every one of these is a true distance field, so the
//     magnitude of its gradient is 1 everywhere outside the shape — checked by finite differences over a grid
//     for all nine primitives;
//   * EXACT DISTANCE: sdCircle and sdBox match hand-computed distances and signs (inside < 0, outside > 0);
//   * SIGN CONSISTENCY: sdTriangle's sign agrees with an independent point-in-triangle test;
//   * BOUNDARY: the field is ~0 on a known boundary point and each shape is negative at its centre;
//   * determinism.
#include "maz/math/Sdf2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Worst |grad|-1 over a grid, considering only points that are safely OUTSIDE (sd > band). For a true SDF
// the exterior gradient magnitude is exactly 1 (distance to a convex set / to the boundary).
static float worstExteriorEikonal(const std::function<float(vec2)>& sd, float band) {
    const float h = 1e-3f;
    float worst = 0.0f;
    for (int ix = -32; ix <= 32; ++ix) {
        for (int iy = -32; iy <= 32; ++iy) {
            const vec2 p(static_cast<float>(ix) * 0.125f, static_cast<float>(iy) * 0.125f);
            if (sd(p) <= band) {
                continue;
            }
            const float gx = (sd(vec2(p.x + h, p.y)) - sd(vec2(p.x - h, p.y))) / (2.0f * h);
            const float gy = (sd(vec2(p.x, p.y + h)) - sd(vec2(p.x, p.y - h))) / (2.0f * h);
            worst = std::max(worst, std::fabs(std::sqrt(gx * gx + gy * gy) - 1.0f));
        }
    }
    return worst;
}

int main() {
    // --- 1. Universal exterior eikonal for all nine primitives. ---
    {
        struct Shape { const char* name; std::function<float(vec2)> sd; };
        const vec2 ta(-1.0f, -0.5f), tb(1.2f, -0.4f), tc(0.1f, 1.3f);
        const std::vector<Shape> shapes = {
            {"circle", [](vec2 p) { return maz::math::sdCircle(p, 1.0f); }},
            {"box", [](vec2 p) { return maz::math::sdBox(p, vec2(1.2f, 0.7f)); }},
            {"roundedBox", [](vec2 p) { return maz::math::sdRoundedBox(p, vec2(1.2f, 0.7f), 0.3f); }},
            {"segment", [](vec2 p) { return maz::math::sdSegment(p, vec2(-1.0f, 0.2f), vec2(1.4f, -0.3f)); }},
            {"orientedBox", [](vec2 p) { return maz::math::sdOrientedBox(p, vec2(-1.0f, 0.3f), vec2(1.5f, -0.2f), 0.6f); }},
            {"equilateral", [](vec2 p) { return maz::math::sdEquilateralTriangle(p, 1.0f); }},
            {"triangle", [ta, tb, tc](vec2 p) { return maz::math::sdTriangle(p, ta, tb, tc); }},
            {"hexagon", [](vec2 p) { return maz::math::sdHexagon(p, 1.0f); }},
            {"pie", [](vec2 p) { return maz::math::sdPie(p, 1.3f, 0.9f); }},
        };
        for (const auto& s : shapes) {
            const float worst = worstExteriorEikonal(s.sd, 0.2f);
            CHECK(worst < 1.5e-2f, "the exterior gradient magnitude is 1 (a true distance field)");
            if (worst >= 1.5e-2f) std::printf("   (%s worst eikonal error = %.4f)\n", s.name, worst);
        }
    }

    // --- 2. Circle & box: exact distances and signs. ---
    {
        CHECK(std::fabs(maz::math::sdCircle(vec2(0, 0), 2.0f) + 2.0f) < 1e-6f, "circle centre distance is -r");
        CHECK(std::fabs(maz::math::sdCircle(vec2(5, 0), 2.0f) - 3.0f) < 1e-6f, "circle at distance 5 is 5-r=3");
        CHECK(std::fabs(maz::math::sdCircle(vec2(2, 0), 2.0f)) < 1e-6f, "circle boundary is 0");
        const vec2 b(1.0f, 2.0f);
        CHECK(std::fabs(maz::math::sdBox(vec2(0, 0), b) + 1.0f) < 1e-6f, "box centre distance is -min(half-extent)");
        CHECK(std::fabs(maz::math::sdBox(vec2(4, 0), b) - 3.0f) < 1e-6f, "box to the +x side is 4-1=3");
        CHECK(std::fabs(maz::math::sdBox(vec2(1, 2), b)) < 1e-6f, "box corner is on the boundary (0)");
        // Diagonal outside a corner: distance to the corner point.
        CHECK(std::fabs(maz::math::sdBox(vec2(4, 6), b) - std::sqrt(9.0f + 16.0f)) < 1e-5f,
              "box beyond a corner is the distance to that corner");
    }

    // --- 3. Triangle sign agrees with an independent point-in-triangle test. ---
    {
        const vec2 p0(-1.0f, -0.5f), p1(1.2f, -0.4f), p2(0.1f, 1.3f);
        auto edgeSign = [](vec2 a, vec2 b, vec2 q) { return (b.x - a.x) * (q.y - a.y) - (b.y - a.y) * (q.x - a.x); };
        int mismatch = 0, insideN = 0;
        for (int ix = -20; ix <= 20; ++ix)
            for (int iy = -20; iy <= 20; ++iy) {
                const vec2 q(static_cast<float>(ix) * 0.1f, static_cast<float>(iy) * 0.1f);
                const float d = maz::math::sdTriangle(q, p0, p1, p2);
                const float s0 = edgeSign(p0, p1, q), s1 = edgeSign(p1, p2, q), s2 = edgeSign(p2, p0, q);
                const bool inside = (s0 <= 0 && s1 <= 0 && s2 <= 0) || (s0 >= 0 && s1 >= 0 && s2 >= 0);
                if (std::fabs(d) < 1e-3f) continue; // skip the ambiguous boundary band
                if (inside) ++insideN;
                if ((d < 0.0f) != inside) ++mismatch;
            }
        CHECK(insideN > 20, "the grid exercises interior triangle points");
        CHECK(mismatch == 0, "sdTriangle's sign matches an independent point-in-triangle test");
    }

    // --- 4. Each shape is negative at its centre / apex-interior point. ---
    {
        CHECK(maz::math::sdCircle(vec2(0, 0), 1.0f) < 0.0f, "circle is negative at the centre");
        CHECK(maz::math::sdBox(vec2(0, 0), vec2(1, 1)) < 0.0f, "box is negative at the centre");
        CHECK(maz::math::sdRoundedBox(vec2(0, 0), vec2(1, 1), 0.3f) < 0.0f, "rounded box is negative at the centre");
        CHECK(maz::math::sdEquilateralTriangle(vec2(0, 0), 1.0f) < 0.0f, "equilateral triangle is negative at the centre");
        CHECK(maz::math::sdHexagon(vec2(0, 0), 1.0f) < 0.0f, "hexagon is negative at the centre");
        CHECK(maz::math::sdPie(vec2(0.0f, 0.6f), 1.3f, 0.9f) < 0.0f, "pie is negative inside the sector");
        CHECK(maz::math::sdPie(vec2(1.2f, -0.6f), 1.3f, 0.9f) > 0.0f, "pie is positive outside the sector");
    }

    // --- 5. Determinism. ---
    {
        CHECK(maz::math::sdHexagon(vec2(0.3f, 0.4f), 1.0f) == maz::math::sdHexagon(vec2(0.3f, 0.4f), 1.0f),
              "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("sdf2d: OK — eikonal (9 shapes), exact circle/box, triangle sign, interior, determinism.\n");
        return 0;
    }
    std::printf("sdf2d: %d failure(s).\n", g_fail);
    return 1;
}
