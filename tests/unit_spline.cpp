// Unit tests for maz::math::CatmullRomSpline — a uniform Catmull-Rom spline through
// its control points. Position/tangent are float math, so comparisons use an approx
// tolerance, but the inputs are chosen exactly representable (integer/half control
// points) so the tolerance is trivially satisfied. Interpolation passes through every
// control point exactly (t==0/t==1 at integer u), interior segments reproduce a
// straight line for collinear equally-spaced points, out-of-range u clamps to the
// endpoints, and sample() yields a boundary-sharing polyline. Pure C++, no GPU/display.

#include "maz/math/Spline.hpp"

#include <cstdio>
#include <cmath>

using maz::math::CatmullRomSpline;
using maz::math::vec3;

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
    // --- 1. PASSES THROUGH CONTROL POINTS EXACTLY ----------------------------
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 2.0f, 0.0f));
        s.addPoint(vec3(3.0f, 1.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        check(vapprox(s.evaluate(0.0f), vec3(0.0f, 0.0f, 0.0f)), "evaluate(0) -> control point 0");
        check(vapprox(s.evaluate(1.0f), vec3(1.0f, 2.0f, 0.0f)), "evaluate(1) -> control point 1");
        check(vapprox(s.evaluate(2.0f), vec3(3.0f, 1.0f, 0.0f)), "evaluate(2) -> control point 2");
        check(vapprox(s.evaluate(3.0f), vec3(4.0f, 0.0f, 0.0f)), "evaluate(3) -> control point 3");
    }

    // --- 2. INTERIOR SEGMENT IS LINEAR FOR COLLINEAR EQUALLY-SPACED POINTS ----
    // Only interior segments (real neighbors on both sides) reproduce the straight
    // line. The FIRST/LAST segments use a clamped endpoint, so are NOT exactly
    // linear — do NOT assert evaluate(0.5)==(0.5,0,0); test only interior segments.
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 0.0f, 0.0f));
        s.addPoint(vec3(2.0f, 0.0f, 0.0f));
        s.addPoint(vec3(3.0f, 0.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        check(vapprox(s.evaluate(1.5f), vec3(1.5f, 0.0f, 0.0f)), "interior evaluate(1.5) -> (1.5,0,0)");
        check(vapprox(s.evaluate(2.5f), vec3(2.5f, 0.0f, 0.0f)), "interior evaluate(2.5) -> (2.5,0,0)");
        check(vapprox(s.evaluate(3.0f), vec3(3.0f, 0.0f, 0.0f)), "evaluate(3.0) -> (3,0,0)");
    }

    // --- 3. CLAMP OUT OF RANGE -----------------------------------------------
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 2.0f, 0.0f));
        s.addPoint(vec3(3.0f, 1.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        check(vapprox(s.evaluate(-5.0f), vec3(0.0f, 0.0f, 0.0f)), "evaluate(-5) clamps to first point");
        check(vapprox(s.evaluate(1000.0f), vec3(4.0f, 0.0f, 0.0f)), "evaluate(1000) clamps to last point");
    }

    // --- 4. COUNTS -----------------------------------------------------------
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 0.0f, 0.0f));
        s.addPoint(vec3(2.0f, 0.0f, 0.0f));
        s.addPoint(vec3(3.0f, 0.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        check(s.pointCount() == 5, "pointCount() == 5");
        check(s.segmentCount() == 4, "segmentCount() == 4");

        CatmullRomSpline one;
        one.addPoint(vec3(7.0f, 8.0f, 9.0f));
        check(one.segmentCount() == 0, "1-point spline segmentCount() == 0");
        check(vapprox(one.evaluate(0.0f), vec3(7.0f, 8.0f, 9.0f)), "1-point evaluate(0) -> that point");
        check(vapprox(one.evaluate(5.0f), vec3(7.0f, 8.0f, 9.0f)), "1-point evaluate(5) -> that point");

        CatmullRomSpline two;
        two.addPoint(vec3(0.0f, 0.0f, 0.0f));
        two.addPoint(vec3(2.0f, 0.0f, 0.0f));
        check(two.segmentCount() == 1, "2-point spline segmentCount() == 1");
        check(vapprox(two.evaluate(0.0f), vec3(0.0f, 0.0f, 0.0f)), "2-point evaluate(0) -> p0");
        check(vapprox(two.evaluate(1.0f), vec3(2.0f, 0.0f, 0.0f)), "2-point evaluate(1) -> p1");
    }

    // --- 5. TANGENT DIRECTION ON THE LINEAR INTERIOR SEGMENT ------------------
    // For seg=2 (u=2.0, t=0): p0=m[1]=(1,0,0), p2=m[3]=(3,0,0) -> 0.5*(p2-p0)=(1,0,0).
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 0.0f, 0.0f));
        s.addPoint(vec3(2.0f, 0.0f, 0.0f));
        s.addPoint(vec3(3.0f, 0.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        vec3 tg = s.tangent(2.0f);
        check(approx(tg.y, 0.0f), "tangent(2.0) has zero y");
        check(approx(tg.z, 0.0f), "tangent(2.0) has zero z");
        check(tg.x > 0.0f, "tangent(2.0) points in +x");
        check(vapprox(tg, vec3(1.0f, 0.0f, 0.0f)), "tangent(2.0) == (1,0,0)");
    }

    // --- 6. sample() POLYLINE ------------------------------------------------
    {
        CatmullRomSpline s;
        s.addPoint(vec3(0.0f, 0.0f, 0.0f));
        s.addPoint(vec3(1.0f, 2.0f, 0.0f));
        s.addPoint(vec3(3.0f, 1.0f, 0.0f));
        s.addPoint(vec3(4.0f, 0.0f, 0.0f));
        std::vector<vec3> poly = s.sample(4);
        check(poly.size() == 3 * 4 + 1, "sample(4) size == 3*4+1 == 13");
        check(vapprox(poly.front(), vec3(0.0f, 0.0f, 0.0f)), "sample front() == first control point");
        check(vapprox(poly.back(), vec3(4.0f, 0.0f, 0.0f)), "sample back() == last control point");

        CatmullRomSpline one;
        one.addPoint(vec3(1.0f, 1.0f, 1.0f));
        check(one.sample(4).size() == 1, "1-point spline sample -> size 1");

        CatmullRomSpline empty;
        check(empty.sample(4).size() == 0, "empty spline sample -> size 0");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
