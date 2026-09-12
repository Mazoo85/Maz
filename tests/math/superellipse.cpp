// tests/math/superellipse.cpp — verifies the superellipse / squircle generator (math Superellipse.hpp).
// Ground truths, deterministic (fixed shapes, no <random>, no clock):
//   * IMPLICIT EQUATION (the airtight oracle): every point returned by superellipsePoint / the polyline
//     satisfies |x/a|^n + |y/b|^n == 1 to floating point, for a range of exponents (astroid, ellipse,
//     squircle, near-rectangle) and non-equal axes;
//   * n == 2 reproduces the ELLIPSE — points satisfy (x/a)^2 + (y/b)^2 = 1 and the ring area equals π·a·b;
//   * MONOTONE NESTING: the inscribed ellipse ⊂ squircle (n=4) ⊂ bounding box — the squircle's diagonal
//     corner sticks out past the ellipse but stays inside the box;
//   * large n drives the corner (t=π/4) toward the box corner (a, b);
//   * central symmetry P(t) == −P(t+π); superellipseContains agrees with the boundary; determinism.
#include "maz/math/Superellipse.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float implicit(float a, float b, float n, const vec2& p) {
    return std::pow(std::fabs(p.x / a), n) + std::pow(std::fabs(p.y / b), n);
}

static float ringArea(const std::vector<vec2>& poly) {
    float s = 0.0f;
    const std::size_t m = poly.size();
    for (std::size_t i = 0; i < m; ++i) {
        const vec2& p = poly[i];
        const vec2& q = poly[(i + 1) % m];
        s += p.x * q.y - q.x * p.y;
    }
    return 0.5f * std::fabs(s);
}

int main() {
    const float pi = 3.14159265358979324f;

    // --- 1. Implicit equation holds for many exponents and axes. ---
    {
        const float exps[] = {0.7f, 1.0f, 2.0f, 4.0f, 8.0f, 20.0f};
        const float a = 3.0f, b = 1.7f;
        float worst = 0.0f;
        for (float n : exps) {
            for (int i = 0; i < 360; ++i) {
                const float t = 2.0f * pi * static_cast<float>(i) / 360.0f;
                const vec2 p = maz::math::superellipsePoint(a, b, n, t);
                worst = std::max(worst, std::fabs(implicit(a, b, n, p) - 1.0f));
            }
        }
        CHECK(worst < 2e-3f, "every superellipse point satisfies |x/a|^n + |y/b|^n = 1");
    }

    // --- 2. n = 2 is the ellipse: area = pi*a*b. ---
    {
        const float a = 2.5f, b = 1.25f;
        const auto ring = maz::math::superellipsePolyline(a, b, 2.0f, 2048);
        const float area = ringArea(ring);
        CHECK(std::fabs(area - pi * a * b) < 1e-2f, "the n=2 superellipse encloses the ellipse area pi*a*b");
    }

    // --- 3. Nesting: ellipse (n=2) area < squircle (n=4) area < bounding box (2a*2b). ---
    {
        const float r = 2.0f;
        const float ellipseArea = ringArea(maz::math::superellipsePolyline(r, r, 2.0f, 2048));
        const float squircleArea = ringArea(maz::math::squircle(r, 2048));
        const float boxArea = (2.0f * r) * (2.0f * r);
        CHECK(ellipseArea < squircleArea, "the squircle is larger than the inscribed circle");
        CHECK(squircleArea < boxArea, "the squircle stays inside the bounding square");
        // The squircle's diagonal corner sticks out past the circle of radius r.
        const vec2 corner = maz::math::superellipsePoint(r, r, 4.0f, pi / 4.0f);
        CHECK(std::sqrt(corner.x * corner.x + corner.y * corner.y) > r,
              "the squircle corner reaches past the inscribed circle");
        CHECK(corner.x < r && corner.y < r, "the squircle corner stays inside the bounding box");
    }

    // --- 4. Large exponent approaches the box corner. ---
    {
        const float a = 1.0f, b = 1.0f;
        const vec2 c = maz::math::superellipsePoint(a, b, 64.0f, pi / 4.0f);
        CHECK(c.x > 0.97f && c.y > 0.97f, "a high-exponent superellipse corner approaches the box corner");
    }

    // --- 5. Central symmetry + containment predicate + determinism. ---
    {
        const float a = 2.0f, b = 3.0f, n = 3.5f;
        for (int i = 0; i < 32; ++i) {
            const float t = 2.0f * pi * static_cast<float>(i) / 32.0f;
            const vec2 p = maz::math::superellipsePoint(a, b, n, t);
            const vec2 q = maz::math::superellipsePoint(a, b, n, t + pi);
            // Analytically P(t) = -P(t+pi) exactly; the tolerance covers cos/sin rounding amplified by the
            // fractional power near the axis cusps (worst ~3e-4 here).
            CHECK(std::fabs(p.x + q.x) < 1.5e-3f && std::fabs(p.y + q.y) < 1.5e-3f,
                  "P(t) = -P(t+pi): the superellipse is centrally symmetric");
            // Boundary point is contained; the same point scaled out by 1.2 is not.
            CHECK(maz::math::superellipseContains(a, b, n, p), "a boundary point is reported inside");
            CHECK(!maz::math::superellipseContains(a, b, n, vec2(p.x * 1.2f, p.y * 1.2f)),
                  "a point scaled beyond the boundary is reported outside");
        }
        CHECK(maz::math::superellipsePoint(a, b, n, 0.9f).x == maz::math::superellipsePoint(a, b, n, 0.9f).x,
              "identical inputs produce identical points");
    }

    if (g_fail == 0) {
        std::printf("superellipse: OK — implicit equation, ellipse area, squircle nesting, box limit, "
                    "symmetry, determinism.\n");
        return 0;
    }
    std::printf("superellipse: %d failure(s).\n", g_fail);
    return 1;
}
