// tests/math/superformula.cpp — verifies the Gielis superformula (math Superformula.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * SUPERELLIPSE IDENTITY (airtight): with m=4, a=b=1, n1=n2=n3=n the point (x,y) satisfies the exact
//     implicit superellipse equation |x|^n + |y|^n = 1 for every angle — tying the generator to the existing
//     Superellipse.hpp and pinning the whole formula down algebraically;
//   * CIRCLE LIMIT: the n=2 sub-case is the unit circle (radius 1 in every direction);
//   * RADIAL PERIODICITY (airtight): r(θ) = r(θ + 8π/m) for arbitrary parameters (the angular term repeats);
//   * CONSTRUCTION: superformulaPoint = center + r(θ)·(cosθ, sinθ);
//   * closed polyline (no duplicated closing point) + determinism.
#include "maz/math/Superformula.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    const vec2 origin(0.0f, 0.0f);
    const float pi = 3.14159265358979324f;

    // --- 1. Superellipse identity: m=4, a=b=1, n1=n2=n3=n  =>  |x|^n + |y|^n = 1. ---
    {
        float worst = 0.0f;
        for (float n : {1.0f, 2.0f, 2.5f, 4.0f, 6.0f}) {
            for (int i = 0; i < 360; ++i) {
                const float th = static_cast<float>(i) * (pi / 180.0f);
                const vec2 p = maz::math::superformulaPoint(origin, 4.0f, n, n, n, 1.0f, 1.0f, th);
                const float implicit = std::pow(std::fabs(p.x), n) + std::pow(std::fabs(p.y), n);
                worst = std::max(worst, std::fabs(implicit - 1.0f));
            }
        }
        CHECK(worst < 5e-4f, "m=4 superformula satisfies the exact superellipse |x|^n+|y|^n=1");
    }

    // --- 2. Circle limit: m=4, all n=2, a=b=1 => radius 1 everywhere. ---
    {
        float mn = 1e30f, mx = 0.0f;
        for (int i = 0; i < 720; ++i) {
            const float th = static_cast<float>(i) * (pi / 360.0f);
            const float r = maz::math::superformulaRadius(4.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f, th);
            mn = std::min(mn, r);
            mx = std::max(mx, r);
        }
        CHECK(std::fabs(mn - 1.0f) < 1e-5f && std::fabs(mx - 1.0f) < 1e-5f, "the n=2 sub-case is a unit circle");
    }

    // --- 3. Radial periodicity: r(θ) = r(θ + 8π/m) for arbitrary parameters. ---
    {
        const float m = 6.0f, n1 = 0.3f, n2 = 1.7f, n3 = 1.7f, a = 1.0f, b = 1.3f;
        const float period = 8.0f * pi / m;
        float worst = 0.0f;
        for (int i = 0; i < 200; ++i) {
            const float th = static_cast<float>(i) * 0.05f;
            const float r0 = maz::math::superformulaRadius(m, n1, n2, n3, a, b, th);
            const float r1 = maz::math::superformulaRadius(m, n1, n2, n3, a, b, th + period);
            worst = std::max(worst, std::fabs(r0 - r1));
        }
        CHECK(worst < 1e-4f, "the radius is periodic with period 8*pi/m");
    }

    // --- 4. Construction: point = center + r*(cos, sin), with a non-trivial centre. ---
    {
        const vec2 c(3.0f, -2.0f);
        const float m = 5.0f, n1 = 0.4f, n2 = 1.0f, n3 = 1.0f;
        float worst = 0.0f;
        for (int i = 0; i < 100; ++i) {
            const float th = static_cast<float>(i) * 0.06f;
            const float r = maz::math::superformulaRadius(m, n1, n2, n3, 1.0f, 1.0f, th);
            const vec2 got = maz::math::superformulaPoint(c, m, n1, n2, n3, 1.0f, 1.0f, th);
            const vec2 want(c.x + r * std::cos(th), c.y + r * std::sin(th));
            worst = std::max(worst, len(got - want));
        }
        CHECK(worst < 1e-6f, "superformulaPoint = center + r*(cos,sin)");
    }

    // --- 5. Closed polyline: N points, none duplicating index 0; determinism. ---
    {
        const auto poly = maz::math::superformulaPolyline(origin, 3.0f, 4.5f, 10.0f, 10.0f, 1.0f, 1.0f, 256);
        CHECK(poly.size() == 256, "the polyline has exactly `samples` points (open ring)");
        CHECK(len(poly.front() - poly.back()) > 1e-4f, "the last point does not duplicate the first");
        CHECK(maz::math::superformulaPolyline(origin, 3.0f, 4.5f, 10.0f, 10.0f, 1.0f, 1.0f, 128) ==
                  maz::math::superformulaPolyline(origin, 3.0f, 4.5f, 10.0f, 10.0f, 1.0f, 1.0f, 128),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("superformula: OK — superellipse identity, circle limit, periodicity, construction.\n");
        return 0;
    }
    std::printf("superformula: %d failure(s).\n", g_fail);
    return 1;
}
