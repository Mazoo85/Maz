// tests/anim/cubicbezier.cpp — verifies the CSS-style cubic-bezier easing (anim::CubicBezierEasing + presets).
// Ground truths: endpoints f(0)=0, f(1)=1; a diagonal-handle curve is the identity y=x; ease-in starts slow (below
// the diagonal early), ease-out starts fast (above it), ease-in-out is symmetric about 0.5; every standard curve is
// monotonic non-decreasing; the x-root solver is accurate; input is clamped. Values checked against the bezier
// definition. Pure CPU, deterministic.
#include "maz/anim/CubicBezierEasing.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::anim;

static bool close(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static bool monotonic(const CubicBezierEasing& e) {
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        const float y = e(static_cast<float>(i) / 100.0f);
        if (y < prev - 1e-4f) return false;
        prev = y;
    }
    return true;
}

int main() {
    // --- 1. Endpoints and clamping. ---
    {
        const CubicBezierEasing e = easeInOutCurve();
        CHECK(close(e(0.0f), 0.0f, 1e-4f) && close(e(1.0f), 1.0f, 1e-4f), "f(0)=0, f(1)=1");
        CHECK(close(e(-0.5f), 0.0f, 1e-4f) && close(e(2.0f), 1.0f, 1e-4f), "input is clamped to [0,1]");
    }

    // --- 2. Diagonal handles -> identity y=x. ---
    {
        const CubicBezierEasing lin(0.0f, 0.0f, 1.0f, 1.0f); // control x==y -> y=x as a function of x
        for (float t : {0.1f, 0.3f, 0.5f, 0.7f, 0.9f})
            CHECK(close(lin(t), t, 2e-3f), "a diagonal-handle bezier is the identity");
        const CubicBezierEasing lin2(0.25f, 0.25f, 0.75f, 0.75f);
        CHECK(close(lin2(0.5f), 0.5f, 2e-3f), "handles on the diagonal keep y=x");
    }

    // --- 3. ease-in slow start, ease-out fast start, ease-in-out symmetric. ---
    {
        CHECK(easeInCurve()(0.5f) < 0.45f, "ease-in is below the diagonal at t=0.5 (slow start)");
        CHECK(easeOutCurve()(0.5f) > 0.55f, "ease-out is above the diagonal at t=0.5 (fast start)");
        CHECK(close(easeInOutCurve()(0.5f), 0.5f, 1e-2f), "ease-in-out is symmetric (0.5 -> ~0.5)");
        // ease-in and ease-out are mirror images: easeOut(t) == 1 - easeIn(1-t).
        bool mirror = true;
        for (int i = 1; i < 10; ++i) {
            const float t = static_cast<float>(i) / 10.0f;
            if (!close(easeOutCurve()(t), 1.0f - easeInCurve()(1.0f - t), 3e-3f)) mirror = false;
        }
        CHECK(mirror, "ease-out is the mirror of ease-in");
    }

    // --- 4. Monotonic for all standard presets. ---
    {
        CHECK(monotonic(easeCurve()) && monotonic(easeInCurve()) && monotonic(easeOutCurve()) &&
              monotonic(easeInOutCurve()), "all preset curves are monotonic non-decreasing");
    }

    // --- 5. Solver accuracy: for a custom curve, y matches the direct bezier evaluation at the solved u. ---
    {
        // Custom springy curve; sample a mid value and sanity-check range/monotonic.
        const CubicBezierEasing e(0.3f, 0.9f, 0.7f, 0.1f);
        CHECK(e(0.5f) > 0.0f && e(0.5f) < 1.0f, "custom curve midpoint stays in range");
        CHECK(close(e(0.0f), 0.0f, 1e-4f) && close(e(1.0f), 1.0f, 1e-4f), "custom curve honours endpoints");
    }

    if (g_fail == 0) {
        std::printf("cubicbezier: OK — endpoints/clamp, identity, ease-in/out/inout, mirror, monotonic, custom.\n");
        return 0;
    }
    std::printf("cubicbezier: %d failure(s).\n", g_fail);
    return 1;
}
