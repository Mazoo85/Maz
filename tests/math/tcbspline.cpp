// tests/math/tcbspline.cpp — verifies the Kochanek-Bartels (TCB) spline (TcbSpline.hpp).
// Ground truths, deterministic:
//   * a segment interpolates its endpoints exactly (s=0 -> p1, s=1 -> p2);
//   * with Tension=Continuity=Bias=0 the segment is EXACTLY uniform Catmull-Rom (computed independently);
//   * the default outgoing tangent equals the Catmull-Rom central difference 0.5*(next-prev);
//   * Tension=1 zeroes the tangents, so the segment midpoint is the plain average of the endpoints;
//   * Bias leans the curve: +bias pulls an early sample toward the incoming direction vs -bias.
#include "maz/math/TcbSpline.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::TcbParams;
using maz::math::tcbSegment;
using maz::math::tcbTangentOut;
using maz::math::vec2;

static bool vnear(const vec2& a, const vec2& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e;
}

// Independent uniform Catmull-Rom evaluation for the segment p1..p2.
static vec2 catmullRom(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float s) {
    const float s2 = s * s, s3 = s2 * s;
    return vec2(0.5f * (2.0f * p1.x + (p2.x - p0.x) * s +
                        (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * s2 +
                        (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * s3),
                0.5f * (2.0f * p1.y + (p2.y - p0.y) * s +
                        (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * s2 +
                        (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * s3));
}

int main() {
    const vec2 p0(0, 0), p1(1, 2), p2(3, 3), p3(5, 1);

    // --- 1. Endpoint interpolation. ---
    {
        CHECK(vnear(tcbSegment(p0, p1, p2, p3, 0.0f), p1), "TCB segment starts at p1 (s=0)");
        CHECK(vnear(tcbSegment(p0, p1, p2, p3, 1.0f), p2), "TCB segment ends at p2 (s=1)");
    }

    // --- 2. Default TCB == uniform Catmull-Rom (the exact cross-check). ---
    {
        bool match = true;
        for (float s = 0.0f; s <= 1.0f; s += 0.05f) {
            if (!vnear(tcbSegment(p0, p1, p2, p3, s), catmullRom(p0, p1, p2, p3, s), 1e-4f)) match = false;
        }
        CHECK(match, "T=C=B=0 reproduces uniform Catmull-Rom exactly");
    }

    // --- 3. Default outgoing tangent is the central difference. ---
    {
        const vec2 tan = tcbTangentOut(p0, p1, p2, TcbParams{});
        CHECK(vnear(tan, vec2(0.5f * (p2.x - p0.x), 0.5f * (p2.y - p0.y))),
              "default outgoing tangent = 0.5*(next-prev)");
    }

    // --- 4. Tension=1 zeroes tangents -> midpoint is the endpoint average. ---
    {
        TcbParams taut;
        taut.tension = 1.0f;
        const vec2 tan = tcbTangentOut(p0, p1, p2, taut);
        CHECK(vnear(tan, vec2(0, 0)), "tension=1 gives a zero tangent");
        const vec2 mid = tcbSegment(p0, p1, p2, p3, 0.5f, taut);
        CHECK(vnear(mid, vec2(0.5f * (p1.x + p2.x), 0.5f * (p1.y + p2.y))),
              "tension=1 midpoint is the average of the endpoints");
    }

    // --- 5. Bias leans the curve. ---
    {
        TcbParams pos, neg;
        pos.bias = 0.8f;
        neg.bias = -0.8f;
        // The outgoing tangent blends (p1-p0) [incoming dir] and (p2-p1) [outgoing dir]. Positive bias
        // weights the incoming direction more, negative weights the outgoing more.
        const vec2 tPos = tcbTangentOut(p0, p1, p2, pos);
        const vec2 tNeg = tcbTangentOut(p0, p1, p2, neg);
        const vec2 inDir(p1.x - p0.x, p1.y - p0.y);  // (1,2)
        const vec2 outDir(p2.x - p1.x, p2.y - p1.y); // (2,1)
        // +bias -> tangent closer to inDir's shape (more weight on x-from-inDir... check the x/y ratio).
        // Concretely: with +bias the (curr-prev) term dominates; with -bias the (next-curr) term does.
        // Compare the coefficient split via the y component: inDir.y=2 vs outDir.y=1.
        CHECK(tPos.y > tNeg.y, "+bias leans toward the incoming segment (larger y from inDir)");
        CHECK(tNeg.x > tPos.x, "-bias leans toward the outgoing segment (larger x from outDir)");
        (void)inDir;
        (void)outDir;
    }

    if (g_fail == 0) {
        std::printf("tcbspline: OK — endpoint interpolation, Catmull-Rom equivalence, central tangent, "
                    "tension, bias lean.\n");
        return 0;
    }
    std::printf("tcbspline: %d failure(s).\n", g_fail);
    return 1;
}
