// tests/math/catmullrom.cpp — verifies the centripetal Catmull-Rom spline (math::CatmullRomSpline).
// Ground truths, exact/continuous vec2 math, deterministic:
//   * INTERPOLATION: eval(i) returns control point i exactly (the spline passes through every point);
//   * endpoints: eval(0) is the first point, eval(size-1) is the last;
//   * C0 continuity: eval just before and just after an interior knot both approach that control point;
//   * collinear control points produce a collinear curve (a straight line stays straight);
//   * the centripetal parameterization is finite (no NaN) even at a sharp 180° turn and with a
//     duplicated (coincident) control point;
//   * tessellate produces the right number of samples and starts/ends on the endpoints;
//   * determinism.
#include "maz/math/CatmullRomSpline.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::CatmullRomSpline;
using maz::math::vec2;

static bool vnear(const vec2& a, const vec2& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e;
}

int main() {
    CatmullRomSpline sp;
    sp.points = {vec2(0, 0), vec2(1, 2), vec2(3, 2), vec2(4, 0), vec2(6, 1)};

    // --- 1. Interpolation: passes through every control point. ---
    {
        bool ok = true;
        for (int i = 0; i < static_cast<int>(sp.points.size()); ++i) {
            if (!vnear(sp.eval(static_cast<float>(i)), sp.points[static_cast<size_t>(i)])) ok = false;
        }
        CHECK(ok, "spline passes through every control point (eval(i) == points[i])");
    }

    // --- 2. Endpoints. ---
    {
        CHECK(vnear(sp.eval(0.0f), sp.points.front()), "eval(0) is the first point");
        CHECK(vnear(sp.eval(4.0f), sp.points.back()), "eval(n-1) is the last point");
        // Clamping outside the range.
        CHECK(vnear(sp.eval(-2.0f), sp.points.front()), "eval below range clamps to first");
        CHECK(vnear(sp.eval(99.0f), sp.points.back()), "eval above range clamps to last");
    }

    // --- 3. C0 continuity at an interior knot (i=2). ---
    {
        const vec2 before = sp.eval(2.0f - 1e-3f);
        const vec2 after = sp.eval(2.0f + 1e-3f);
        CHECK(vnear(before, sp.points[2], 2e-2f) && vnear(after, sp.points[2], 2e-2f),
              "curve is continuous through the interior control point");
    }

    // --- 4. Collinear control points -> collinear curve. ---
    {
        CatmullRomSpline line;
        line.points = {vec2(0, 0), vec2(1, 0), vec2(2, 0), vec2(3, 0)};
        bool onLine = true;
        for (int k = 0; k <= 30; ++k) {
            const float u = 3.0f * static_cast<float>(k) / 30.0f;
            if (std::fabs(line.eval(u).y) > 1e-3f) onLine = false;
        }
        CHECK(onLine, "a straight line of control points yields a straight curve");
    }

    // --- 5. Robust to a sharp turn and a duplicated point (no NaN). ---
    {
        CatmullRomSpline sharp;
        sharp.points = {vec2(0, 0), vec2(1, 0), vec2(1, 0), vec2(0, 0)}; // duplicate + 180° reversal
        bool finite = true;
        for (int k = 0; k <= 30; ++k) {
            const float u = 3.0f * static_cast<float>(k) / 30.0f;
            const vec2 p = sharp.eval(u);
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) finite = false;
        }
        CHECK(finite, "centripetal spline stays finite through a duplicate/reversal");
    }

    // --- 6. Tessellation count + endpoints. ---
    {
        const auto pts = sp.tessellate(8); // 4 segments * 8 + 1
        CHECK(pts.size() == static_cast<size_t>((sp.points.size() - 1) * 8 + 1),
              "tessellate produces segsPerCount*segments + 1 points");
        CHECK(vnear(pts.front(), sp.points.front()) && vnear(pts.back(), sp.points.back()),
              "tessellation starts and ends on the endpoints");
    }

    // --- 7. Determinism. ---
    {
        CHECK(vnear(sp.eval(1.37f), sp.eval(1.37f)) && sp.eval(2.6f).x == sp.eval(2.6f).x,
              "deterministic evaluation");
    }

    if (g_fail == 0) {
        std::printf("catmullrom: OK — interpolation, endpoints, continuity, collinear, robustness, "
                    "tessellation, determinism.\n");
        return 0;
    }
    std::printf("catmullrom: %d failure(s).\n", g_fail);
    return 1;
}
