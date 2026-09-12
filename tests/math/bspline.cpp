// tests/math/bspline.cpp — verifies the uniform cubic B-spline (math BSpline.hpp).
// Ground truths, hand-computed, deterministic:
//   * a segment's knot points are the classic averages: B(0)=(P0+4P1+P2)/6, B(1)=(P1+4P2+P3)/6;
//   * the tangent at t=0 is the central difference (P2-P0)/2;
//   * all-equal control points give a constant curve (partition of unity);
//   * a concrete segment midpoint matches the hand-computed value;
//   * linear precision: evenly-spaced collinear control points trace the straight line exactly;
//   * convex-hull containment: every sample lies within the bounding box of its 4 control points;
//   * a closed chain is C0-continuous across segment joins and wraps seamlessly;
//   * the analytic tangent matches a central finite difference of the position.
#include "maz/math/BSpline.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::bsplineEval;
using maz::math::bsplineEvalTangent;
using maz::math::bsplinePoint;
using maz::math::bsplineSegmentCount;
using maz::math::bsplineTangent;
using maz::math::vec2;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool vnear(const vec2& a, const vec2& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e;
}

int main() {
    const vec2 P0(0, 0), P1(0, 3), P2(3, 3), P3(3, 0);

    // --- 1. Knot-point averages at t=0 and t=1. ---
    {
        const vec2 b0 = bsplinePoint(P0, P1, P2, P3, 0.0f);
        CHECK(vnear(b0, vec2(0.5f, 2.5f)), "B(0) = (P0+4P1+P2)/6");
        const vec2 b1 = bsplinePoint(P0, P1, P2, P3, 1.0f);
        CHECK(vnear(b1, vec2(2.5f, 2.5f)), "B(1) = (P1+4P2+P3)/6");
    }

    // --- 2. Central-difference tangent at t=0. ---
    {
        const vec2 tg = bsplineTangent(P0, P1, P2, P3, 0.0f);
        CHECK(vnear(tg, vec2(1.5f, 1.5f)), "B'(0) = (P2-P0)/2");
    }

    // --- 3. Concrete midpoint. ---
    {
        const vec2 m = bsplinePoint(P0, P1, P2, P3, 0.5f);
        CHECK(vnear(m, vec2(1.5f, 2.875f)), "hand-computed segment midpoint");
    }

    // --- 4. All-equal control points -> constant. ---
    {
        const vec2 q(7, -2);
        for (float t = 0.0f; t <= 1.0f; t += 0.25f) {
            CHECK(vnear(bsplinePoint(q, q, q, q, t), q), "flat control net gives a constant point");
        }
    }

    // --- 5. Linear precision: collinear even spacing traces the line. ---
    {
        const vec2 a(0, 1), b(1, 1), c(2, 1), d(3, 1); // all y=1, evenly spaced
        for (float t = 0.0f; t <= 1.0f; t += 0.2f) {
            const vec2 p = bsplinePoint(a, b, c, d, t);
            CHECK(near(p.y, 1.0f), "cubic B-spline reproduces a straight line (y stays on it)");
        }
        // x goes linearly from 1 to 2 across the single central segment.
        CHECK(near(bsplinePoint(a, b, c, d, 0.5f).x, 1.5f), "linear param at the segment centre");
    }

    // --- 6. Convex-hull containment. ---
    {
        const float minx = 0.0f, maxx = 3.0f, miny = 0.0f, maxy = 3.0f; // bbox of P0..P3
        for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
            const vec2 p = bsplinePoint(P0, P1, P2, P3, t);
            CHECK(p.x >= minx - 1e-5f && p.x <= maxx + 1e-5f && p.y >= miny - 1e-5f &&
                      p.y <= maxy + 1e-5f,
                  "sample stays inside the control-point bounding box (no overshoot)");
        }
    }

    // --- 7. Segment counts. ---
    {
        std::vector<vec2> five = {P0, P1, P2, P3, vec2(5, 5)};
        CHECK(bsplineSegmentCount(five.size(), false) == 2, "open: n-3 segments");
        CHECK(bsplineSegmentCount(five.size(), true) == 5, "closed: n segments");
        CHECK(bsplineSegmentCount(3, false) == 0, "open needs >= 4 points");
        CHECK(bsplineSegmentCount(2, true) == 0, "closed needs >= 3 points");
    }

    // --- 8. Closed chain: C0 continuity across joins + seamless wrap. ---
    {
        const std::vector<vec2> sq = {{0, 0}, {2, 0}, {2, 2}, {0, 2}}; // closed square net, 4 segments
        const int segs = bsplineSegmentCount(sq.size(), true);
        CHECK(segs == 4, "closed square gives 4 segments");
        for (int s = 0; s < segs; ++s) {
            const vec2 endOfThis = bsplineEval(sq, static_cast<float>(s + 1) - 1e-4f, true);
            const vec2 startOfNext = bsplineEval(sq, static_cast<float>(s + 1) + 1e-4f, true);
            CHECK(vnear(endOfThis, startOfNext, 2e-3f), "closed chain is continuous across joins");
        }
        // Wrap: u=0 and u=segs are the same point on the loop.
        CHECK(vnear(bsplineEval(sq, 0.0f, true), bsplineEval(sq, static_cast<float>(segs), true)),
              "closed chain wraps seamlessly (u=0 == u=n)");
    }

    // --- 9. Analytic tangent vs central finite difference. ---
    {
        const std::vector<vec2> ctrl = {P0, P1, P2, P3, vec2(6, 1), vec2(5, -2)};
        const float h = 1e-3f;
        for (float u = 0.5f; u <= 2.5f; u += 0.5f) {
            const vec2 a = bsplineEval(ctrl, u - h, false);
            const vec2 b = bsplineEval(ctrl, u + h, false);
            const vec2 fd((b.x - a.x) / (2.0f * h), (b.y - a.y) / (2.0f * h));
            const vec2 an = bsplineEvalTangent(ctrl, u, false);
            CHECK(vnear(fd, an, 1e-2f), "analytic tangent matches finite difference");
        }
    }

    if (g_fail == 0) {
        std::printf("bspline: OK — knot averages, central tangent, midpoint, constant net, linear "
                    "precision, convex hull, segment counts, closed continuity, tangent vs FD.\n");
        return 0;
    }
    std::printf("bspline: %d failure(s).\n", g_fail);
    return 1;
}
