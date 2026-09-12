// tests/math/involute.cpp — verifies the involute of a circle (math Involute.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * UNWOUND-STRING IDENTITY (airtight): the free end is exactly baseRadius·t from the tangent point on the
//     base circle, and the string (end - tangentPoint) is PERPENDICULAR to the base radius there — the two
//     properties that make involute the correct gear-tooth profile;
//   * SWEPT ARC LENGTH: the arc length from 0 to t is exactly baseRadius·t²/2 (measured by an independent
//     fine-polyline sum);
//   * RADIUS OF CURVATURE: the local radius of curvature at t equals baseRadius·t (checked with an
//     independent three-point circumradius);
//   * the analytic involuteTangent agrees in direction with the finite-difference tangent;
//   * polyline endpoints + determinism.
#include "maz/math/Involute.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// Independent circumradius of the triangle (a,b,c): R = |ab|·|bc|·|ca| / (2·|area|).
static float circumRadius(const vec2& a, const vec2& b, const vec2& c) {
    const float ab = len(b - a), bc = len(c - b), ca = len(a - c);
    const float area2 = std::fabs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
    return ab * bc * ca / (2.0f * area2);
}

int main() {
    const vec2 c(2.0f, -1.0f);
    const float r = 0.8f;
    const float pi = 3.14159265358979324f;

    // --- 1. Unwound-string identity + perpendicularity (the defining gear property). ---
    {
        float worstLen = 0.0f, worstDot = 0.0f;
        for (int i = 1; i < 400; ++i) {
            const float t = static_cast<float>(i) * 0.02f;
            const vec2 p = maz::math::involutePoint(c, r, t);
            const vec2 tp = maz::math::involuteTangentPoint(c, r, t);
            const vec2 string = p - tp;    // the free part of the unwound string
            const vec2 radius = tp - c;    // base radius to the tangent point
            worstLen = std::max(worstLen, std::fabs(len(string) - r * t));
            // Perpendicular: normalise the dot by the magnitudes so it is a true cosine.
            worstDot = std::max(worstDot, std::fabs((string.x * radius.x + string.y * radius.y) /
                                                    (len(string) * len(radius))));
        }
        CHECK(worstLen < 1e-4f, "the unwound string length equals baseRadius*t");
        CHECK(worstDot < 1e-4f, "the string is perpendicular to the base radius (gear line of action)");
    }

    // --- 2. Swept arc length from 0 to T equals baseRadius*T^2/2. ---
    {
        const float T = 5.0f;
        float total = 0.0f;
        vec2 prev = maz::math::involutePoint(c, r, 0.0f);
        const int n = 400000;
        for (int i = 1; i <= n; ++i) {
            const vec2 p = maz::math::involutePoint(c, r, T * static_cast<float>(i) / static_cast<float>(n));
            total += len(p - prev);
            prev = p;
        }
        CHECK(std::fabs(total - r * T * T * 0.5f) < 2e-3f, "the swept arc length is baseRadius*T^2/2");
    }

    // --- 3. Radius of curvature at t equals baseRadius*t (independent circumradius). ---
    // The step `h` balances two errors in the discrete circumradius oracle: too small and the tiny-triangle
    // area cancels catastrophically in float32; too large and the finite three-point circle deviates from the
    // true osculating circle. h = 0.02 is the well-conditioned middle.
    {
        const float h = 2e-2f;
        float worst = 0.0f;
        for (int i = 5; i < 200; ++i) {
            const float t = static_cast<float>(i) * 0.03f;
            const vec2 a = maz::math::involutePoint(c, r, t - h);
            const vec2 b = maz::math::involutePoint(c, r, t);
            const vec2 d = maz::math::involutePoint(c, r, t + h);
            const float rho = circumRadius(a, b, d);
            worst = std::max(worst, std::fabs(rho - r * t) / (r * t));
        }
        CHECK(worst < 3e-3f, "the radius of curvature at t is baseRadius*t");
    }

    // --- 4. Analytic tangent direction matches the finite-difference tangent. ---
    {
        const float h = 1e-3f;
        float worstCos = 1.0f;
        for (int i = 1; i < 150; ++i) {
            const float t = static_cast<float>(i) * 0.04f;
            const vec2 an = maz::math::involuteTangent(r, t);
            const vec2 p = maz::math::involutePoint(c, r, t - h);
            const vec2 q = maz::math::involutePoint(c, r, t + h);
            const vec2 fd = q - p;
            worstCos = std::min(worstCos, (an.x * fd.x + an.y * fd.y) / (len(an) * len(fd)));
        }
        CHECK(worstCos > 0.9999f, "involuteTangent points the same way as the numerical tangent");
    }

    // --- 5. t=0 starts on the base circle at angle 0; polyline endpoints + determinism. ---
    {
        const vec2 start = maz::math::involutePoint(c, r, 0.0f);
        CHECK(len(start - vec2(c.x + r, c.y)) < 1e-6f, "t=0 sits on the base circle at angle 0");
        const auto poly = maz::math::involutePolyline(c, r, 0.0f, 6.0f * pi, 300);
        CHECK(poly.size() == 301, "the polyline has samples+1 points");
        CHECK(len(poly.back() - maz::math::involutePoint(c, r, 6.0f * pi)) < 1e-4f, "ends at tEnd");
        CHECK(maz::math::involutePolyline(c, r, 0.0f, 6.0f * pi, 64) ==
                  maz::math::involutePolyline(c, r, 0.0f, 6.0f * pi, 64),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("involute: OK — unwound-string identity, arc length, curvature, tangent, determinism.\n");
        return 0;
    }
    std::printf("involute: %d failure(s).\n", g_fail);
    return 1;
}
