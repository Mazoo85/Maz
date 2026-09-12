// tests/math/roulette.cpp — verifies the roulette curve family (math Roulette.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * ASTROID IDENTITY (airtight): a hypocycloid with R = 4r is exactly the astroid x = R·cos^3 t,
//     y = R·sin^3 t — a closed-form the generator must reproduce to float precision;
//   * KNOWN ARC LENGTHS: one cycloid arch has length exactly 8r; the astroid's perimeter is exactly 6R
//     (both measured by a very fine independent polyline sum);
//   * CUSPS: the speed |dP/dt| of the cycloid vanishes at the cusp (t = 0) and is large mid-arch, while a
//     CURTATE trochoid (d < r) never has zero speed;
//   * CLOSURE: an epicycloid with integer R/r returns to its start after one turn;
//   * polyline endpoints + determinism.
#include "maz/math/Roulette.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// Independent arc length of a curve sampled by `f` over [a,b] with N segments.
template <class F>
static float polyLength(F&& f, float a, float b, int n) {
    float total = 0.0f;
    vec2 prev = f(a);
    for (int i = 1; i <= n; ++i) {
        const vec2 p = f(a + (b - a) * static_cast<float>(i) / static_cast<float>(n));
        total += len(p - prev);
        prev = p;
    }
    return total;
}

int main() {
    const float pi = 3.14159265358979324f;

    // --- 1. Astroid identity: hypocycloid(4r, r, t) == (R cos^3 t, R sin^3 t). ---
    {
        const float r = 0.75f, R = 4.0f * r;
        float worst = 0.0f;
        for (int i = 0; i < 500; ++i) {
            const float t = static_cast<float>(i) * (2.0f * pi / 500.0f);
            const vec2 got = maz::math::hypocycloidPoint(R, r, t);
            const float ct = std::cos(t), st = std::sin(t);
            const vec2 want(R * ct * ct * ct, R * st * st * st);
            worst = std::max(worst, len(got - want));
        }
        CHECK(worst < 1e-4f, "hypocycloid(4r,r,t) matches the closed-form astroid to float precision");
    }

    // --- 2. One cycloid arch has length exactly 8r. ---
    {
        const float r = 1.3f;
        const float L = polyLength([&](float t) { return maz::math::cycloidPoint(r, t); }, 0.0f, 2.0f * pi,
                                   200000);
        CHECK(std::fabs(L - 8.0f * r) < 2e-3f, "one cycloid arch has arc length 8r");
    }

    // --- 3. The astroid's perimeter is exactly 6R. ---
    {
        const float r = 0.5f, R = 4.0f * r;
        const float L = polyLength([&](float t) { return maz::math::hypocycloidPoint(R, r, t); }, 0.0f,
                                   2.0f * pi, 200000);
        CHECK(std::fabs(L - 6.0f * R) < 3e-3f, "the astroid (R=4r) has perimeter 6R");
    }

    // --- 4. Cusps: cycloid speed vanishes at t=0 and is large mid-arch; curtate trochoid never stops. ---
    {
        const float r = 1.0f, h = 1e-4f;
        auto speed = [&](float d, float t) {
            const vec2 a = maz::math::trochoidPoint(r, d, t - h);
            const vec2 b = maz::math::trochoidPoint(r, d, t + h);
            return len(b - a) / (2.0f * h);
        };
        CHECK(speed(r, 0.0f) < 1e-2f, "the cycloid's speed vanishes at the cusp (t=0)");
        CHECK(speed(r, pi) > 1.0f, "the cycloid moves fast at the middle of the arch");
        // Curtate (d < r): the wheel point never reaches the ground, so it never momentarily stops.
        float minSpeed = 1e30f;
        for (int i = 0; i < 400; ++i) {
            minSpeed = std::min(minSpeed, speed(0.5f, static_cast<float>(i) * 0.05f));
        }
        CHECK(minSpeed > 0.4f, "a curtate trochoid (d<r) has no cusp — its speed stays positive");
    }

    // --- 5. Closure: an epicycloid with integer R/r returns to its start after one turn. ---
    {
        const float r = 0.6f, R = 3.0f * r; // 3-cusped epicycloid
        const vec2 p0 = maz::math::epicycloidPoint(R, r, 0.0f);
        const vec2 pN = maz::math::epicycloidPoint(R, r, 2.0f * pi);
        CHECK(len(p0 - pN) < 1e-4f, "an integer-ratio epicycloid closes after one full turn");
        // And it is NOT already closed at half a turn (it is a genuine multi-cusp loop).
        const vec2 half = maz::math::epicycloidPoint(R, r, pi);
        CHECK(len(p0 - half) > 0.1f, "the epicycloid is not degenerate (start != halfway point)");
    }

    // --- 6. Deltoid (R=3r) is closed and three-cusped; spot-check a known cusp location. ---
    {
        const float r = 1.0f, R = 3.0f;
        // Deltoid cusps sit on the fixed circle at radius (R - 2r)+... — simplest check: t=0 gives the
        // rightmost cusp at x = (R-r) + r = R, y = 0.
        const vec2 cusp = maz::math::hypocycloidPoint(R, r, 0.0f);
        CHECK(std::fabs(cusp.x - R) < 1e-5f && std::fabs(cusp.y) < 1e-5f,
              "the deltoid's t=0 cusp sits at (R, 0)");
    }

    // --- 7. Polyline endpoints + determinism. ---
    {
        const auto poly = maz::math::hypotrochoidPolyline(5.0f, 3.0f, 5.0f, 0.0f, 6.0f * pi, 300);
        CHECK(poly.size() == 301, "the polyline has samples+1 points");
        CHECK(len(poly.front() - maz::math::hypotrochoidPoint(5.0f, 3.0f, 5.0f, 0.0f)) < 1e-5f,
              "starts at tStart");
        CHECK(len(poly.back() - maz::math::hypotrochoidPoint(5.0f, 3.0f, 5.0f, 6.0f * pi)) < 1e-3f,
              "ends at tEnd");
        CHECK(maz::math::epitrochoidPolyline(4.0f, 1.0f, 2.0f, 0.0f, 2.0f * pi, 64) ==
                  maz::math::epitrochoidPolyline(4.0f, 1.0f, 2.0f, 0.0f, 2.0f * pi, 64),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("roulette: OK — astroid identity, arch/perimeter lengths, cusps, closure, determinism.\n");
        return 0;
    }
    std::printf("roulette: %d failure(s).\n", g_fail);
    return 1;
}
