// tests/math/logspiral.cpp — verifies the logarithmic (equiangular) spiral (math LogSpiral.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * EQUIANGULAR (the defining, airtight property): the angle between the radius (centre→point) and the
//     tangent — measured NUMERICALLY by finite differences, independent of the library's tangent function —
//     is the SAME at every point along the spiral, and equals acos(b/sqrt(b^2+1));
//   * SELF-SIMILAR: advancing the angle by one full turn multiplies the radius by exactly e^(2*pi*b);
//   * the analytic logSpiralTangent agrees with the finite-difference tangent;
//   * b = 0 degenerates to a circle of radius a; determinism.
#include "maz/math/LogSpiral.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    const vec2 c(1.0f, -2.0f);
    const float a = 0.5f, b = 0.15f;
    const float pi = 3.14159265358979324f;

    // --- 1. Equiangular: constant radius-to-tangent angle (finite-difference tangent). ---
    {
        const float expected = std::acos(b / std::sqrt(b * b + 1.0f));
        float worst = 0.0f;
        for (int i = 0; i < 400; ++i) {
            const float th = static_cast<float>(i) * 0.05f;
            const vec2 p = maz::math::logSpiralPoint(c, a, b, th);
            const vec2 p2 = maz::math::logSpiralPoint(c, a, b, th + 1e-3f);
            const vec2 tan(p2.x - p.x, p2.y - p.y);
            const vec2 rad(p.x - c.x, p.y - c.y);
            const float cosang = (rad.x * tan.x + rad.y * tan.y) / (len(rad) * len(tan));
            worst = std::max(worst, std::fabs(std::acos(cosang) - expected));
        }
        CHECK(worst < 3e-3f, "the radius-to-tangent angle is constant along the spiral (equiangular)");
    }

    // --- 2. Self-similarity: one full turn scales the radius by e^(2*pi*b). ---
    {
        const float ratio = std::exp(b * 2.0f * pi);
        float worst = 0.0f;
        for (int i = 0; i < 20; ++i) {
            const float th = static_cast<float>(i) * 0.3f;
            const vec2 p0 = maz::math::logSpiralPoint(vec2(0, 0), a, b, th);
            const vec2 p1 = maz::math::logSpiralPoint(vec2(0, 0), a, b, th + 2.0f * pi);
            worst = std::max(worst, std::fabs(len(p1) / len(p0) - ratio));
        }
        CHECK(worst < 1e-3f, "advancing one turn multiplies the radius by e^(2*pi*b)");
    }

    // --- 3. Analytic tangent matches the finite-difference tangent (direction). ---
    {
        float worstCos = 1.0f;
        for (int i = 0; i < 100; ++i) {
            const float th = static_cast<float>(i) * 0.1f;
            const vec2 an = maz::math::logSpiralTangent(a, b, th);
            const vec2 p = maz::math::logSpiralPoint(vec2(0, 0), a, b, th);
            const vec2 p2 = maz::math::logSpiralPoint(vec2(0, 0), a, b, th + 1e-3f);
            const vec2 fd(p2.x - p.x, p2.y - p.y);
            const float cosang = (an.x * fd.x + an.y * fd.y) / (len(an) * len(fd));
            worstCos = std::min(worstCos, cosang);
        }
        CHECK(worstCos > 0.9999f, "logSpiralTangent points the same way as the numerical tangent");
    }

    // --- 4. b = 0 is a circle of radius a. ---
    {
        float mn = 1e30f, mx = 0.0f;
        for (int i = 0; i < 200; ++i) {
            const vec2 p = maz::math::logSpiralPoint(vec2(0, 0), a, 0.0f, static_cast<float>(i) * 0.1f);
            const float r = len(p);
            mn = std::min(mn, r);
            mx = std::max(mx, r);
        }
        CHECK(std::fabs(mn - a) < 1e-5f && std::fabs(mx - a) < 1e-5f, "b=0 gives a circle of radius a");
    }

    // --- 5. Polyline endpoints + determinism. ---
    {
        const auto poly = maz::math::logSpiralPolyline(c, a, b, 0.0f, 4.0f * pi, 200);
        CHECK(poly.size() == 201, "the polyline has samples+1 points");
        CHECK(len(poly.front() - maz::math::logSpiralPoint(c, a, b, 0.0f)) < 1e-5f, "starts at theta=start");
        CHECK(len(poly.back() - maz::math::logSpiralPoint(c, a, b, 4.0f * pi)) < 1e-4f, "ends at theta=end");
        CHECK(maz::math::logSpiralPolyline(c, a, b, 0.0f, 4.0f * pi, 50) ==
                  maz::math::logSpiralPolyline(c, a, b, 0.0f, 4.0f * pi, 50),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("logspiral: OK — equiangular, self-similar, tangent, circle limit, determinism.\n");
        return 0;
    }
    std::printf("logspiral: %d failure(s).\n", g_fail);
    return 1;
}
