// Unit tests for maz::math::CubicBezier — a handle-based cubic Bezier curve (four
// control points; the curve passes through the endpoints p0/p3 and is pulled toward
// the handles p1/p2 which it does NOT interpolate). Position/tangent are float math,
// so comparisons use an approx tolerance, but inputs are chosen exactly representable
// (integer/half/quarter control points) so the tolerance is trivially satisfied.
// Endpoints collapse to p0/p3 exactly at t=0/t=1, evenly-spaced collinear controls
// reproduce a straight line, the tangent is 3x the handle vector at each end, the
// free functions match the methods, sample() yields a first==p0/last==p3 polyline,
// and out-of-range t extrapolates (finite, no assert). Pure C++, no GPU/display.

#include "maz/math/Bezier.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }

bool vapprox(vec3 a, vec3 b) {
    return approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z);
}

} // namespace

int main() {
    // --- 1. ENDPOINTS ARE EXACT (t=0 -> p0, t=1 -> p3) -----------------------
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 2.0f, 0.0f),
                       vec3(2.0f, 2.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        check(vapprox(b.evaluate(0.0f), vec3(0.0f, 0.0f, 0.0f)), "evaluate(0) == p0");
        check(vapprox(b.evaluate(1.0f), vec3(3.0f, 0.0f, 0.0f)), "evaluate(1) == p3");
    }

    // --- 2. COLLINEAR EVENLY-SPACED -> STRAIGHT LINE -------------------------
    // Controls on the x-axis at 0,1,2,3: evaluate(0.5) == (1.5,0,0).
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f),
                       vec3(2.0f, 0.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        check(vapprox(b.evaluate(0.5f), vec3(1.5f, 0.0f, 0.0f)), "collinear evaluate(0.5) == (1.5,0,0)");
        vec3 q = b.evaluate(0.25f);
        check(approx(q.y, 0.0f) && approx(q.z, 0.0f), "collinear evaluate(0.25) on x-axis");
        vec3 r = b.evaluate(0.75f);
        check(approx(r.y, 0.0f) && approx(r.z, 0.0f), "collinear evaluate(0.75) on x-axis");
    }

    // --- 3. SYMMETRIC ARC MIDPOINT ------------------------------------------
    // Controls (0,0,0),(0,1,0),(1,1,0),(1,0,0): evaluate(0.5) == (0.5,0.75,0).
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f),
                       vec3(1.0f, 1.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f) };
        check(vapprox(b.evaluate(0.5f), vec3(0.5f, 0.75f, 0.0f)), "symmetric arc evaluate(0.5) == (0.5,0.75,0)");
    }

    // --- 4. TANGENT AT ENDPOINTS (3x the handle vector) ---------------------
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 2.0f, 0.0f),
                       vec3(2.0f, 2.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        check(vapprox(b.tangent(0.0f), vec3(3.0f, 6.0f, 0.0f)), "tangent(0) == 3*(p1-p0) == (3,6,0)");
        check(vapprox(b.tangent(1.0f), vec3(3.0f, -6.0f, 0.0f)), "tangent(1) == 3*(p3-p2) == (3,-6,0)");
    }

    // --- 5. TANGENT ON THE STRAIGHT LINE (constant (3,0,0)) -----------------
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f),
                       vec3(2.0f, 0.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        vec3 tg = b.tangent(0.5f);
        check(tg.x > 0.0f, "collinear tangent(0.5) points +x");
        check(approx(tg.y, 0.0f) && approx(tg.z, 0.0f), "collinear tangent(0.5) y==z==0");
        check(vapprox(tg, vec3(3.0f, 0.0f, 0.0f)), "collinear tangent(0.5) == (3,0,0)");
    }

    // --- 6. FREE FUNCTION == METHOD -----------------------------------------
    {
        vec3 p0(0.0f, 0.0f, 0.0f), p1(1.0f, 2.0f, 0.0f), p2(2.0f, 2.0f, 0.0f), p3(3.0f, 0.0f, 0.0f);
        CubicBezier b{ p0, p1, p2, p3 };
        check(vapprox(cubicBezier(p0, p1, p2, p3, 0.5f), b.evaluate(0.5f)), "cubicBezier() == evaluate()");
        check(vapprox(cubicBezierTangent(p0, p1, p2, p3, 0.5f), b.tangent(0.5f)), "cubicBezierTangent() == tangent()");
    }

    // --- 7. sample() POLYLINE -----------------------------------------------
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 2.0f, 0.0f),
                       vec3(2.0f, 2.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        std::vector<vec3> poly = b.sample(4);
        check(poly.size() == 5, "sample(4) size == 5");
        check(vapprox(poly.front(), vec3(0.0f, 0.0f, 0.0f)), "sample front() == p0");
        check(vapprox(poly.back(), vec3(3.0f, 0.0f, 0.0f)), "sample back() == p3");
    }

    // --- 8. EXTRAPOLATION IS DEFINED (no assert on t range) -----------------
    {
        CubicBezier b{ vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 2.0f, 0.0f),
                       vec3(2.0f, 2.0f, 0.0f), vec3(3.0f, 0.0f, 0.0f) };
        vec3 e = b.evaluate(2.0f);
        check(std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z), "evaluate(2.0) extrapolates finite");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
