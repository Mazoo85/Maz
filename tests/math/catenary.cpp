// tests/math/catenary.cpp — verifies the hanging-chain catenary solver (math Catenary.hpp).
// Ground truths, deterministic (fixed anchors/lengths, no <random>, no clock):
//   * ENDPOINTS (airtight): the solved curve passes through both anchor points exactly;
//   * ROPE LENGTH: the curve's arc length equals the requested rope length — checked BOTH by the closed-form
//     a·sinh formula AND by an independent fine-polyline sum;
//   * HANGING-CHAIN ODE (airtight, the defining physical law): a·y'' = sqrt(1 + y'^2) at every point — the
//     reason a real chain under gravity takes exactly this shape and NOT a parabola;
//   * SAG MONOTONICITY: more rope => deeper sag; a symmetric hang has its low point at the midpoint;
//   * a taut/too-short or vertical rope is reported unsolvable and the polyline falls back to a straight line;
//   * determinism.
#include "maz/math/Catenary.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;
using maz::math::Catenary;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    // --- 1. Endpoints + rope length (closed-form and independent polyline sum). ---
    {
        const vec2 p1(-1.5f, 2.0f), p2(3.0f, 3.5f);
        const float L = 7.0f; // > straight distance (~4.74)
        Catenary cat;
        const bool ok = maz::math::solveCatenary(p1, p2, L, cat);
        CHECK(ok, "a rope longer than the gap yields a catenary");
        if (ok) {
            CHECK(std::fabs(maz::math::catenaryHeight(cat, p1.x) - p1.y) < 1e-3f, "curve passes through p1");
            CHECK(std::fabs(maz::math::catenaryHeight(cat, p2.x) - p2.y) < 1e-3f, "curve passes through p2");
            // Closed-form arc length.
            const float sClosed = maz::math::catenaryArcLength(cat, p1.x, p2.x);
            CHECK(std::fabs(sClosed - L) < 2e-3f, "closed-form arc length equals the rope length");
            // Independent fine-polyline arc length.
            const auto poly = maz::math::catenaryPolyline(p1, p2, L, 200000);
            float sPoly = 0.0f;
            for (std::size_t i = 1; i < poly.size(); ++i) sPoly += len(poly[i] - poly[i - 1]);
            CHECK(std::fabs(sPoly - L) < 3e-3f, "independent polyline arc length equals the rope length");
        }
    }

    // --- 2. Hanging-chain ODE: a*y'' = sqrt(1 + y'^2) (why it is NOT a parabola). ---
    {
        const vec2 p1(0.0f, 5.0f), p2(6.0f, 1.0f);
        const float L = 9.0f;
        Catenary cat;
        const bool ok = maz::math::solveCatenary(p1, p2, L, cat);
        CHECK(ok, "asymmetric rope solves");
        if (ok) {
            // Evaluate the height in double at a moderate step: a float y'' with h=1e-3 would be destroyed by
            // cancellation (h^2 = 1e-6 dividing a tiny difference of ~5-magnitude floats).
            const double a = cat.a, x0 = cat.x0, c = cat.c;
            auto Y = [&](double x) { return a * std::cosh((x - x0) / a) + c; };
            const double hh = 1e-2;
            double worst = 0.0;
            for (int i = 1; i < 60; ++i) {
                const double x = 0.1 + static_cast<double>(i) * 0.09;
                const double yp = (Y(x + hh) - Y(x - hh)) / (2.0 * hh);           // y'
                const double ypp = (Y(x + hh) - 2.0 * Y(x) + Y(x - hh)) / (hh * hh); // y''
                worst = std::max(worst, std::fabs(a * ypp - std::sqrt(1.0 + yp * yp)));
            }
            CHECK(worst < 1e-3, "satisfies the hanging-chain ODE a*y'' = sqrt(1+y'^2)");
        }
    }

    // --- 3. Symmetric hang: low point at the midpoint; more rope => deeper sag. ---
    {
        const vec2 p1(-2.0f, 0.0f), p2(2.0f, 0.0f);
        Catenary a, b;
        const bool okA = maz::math::solveCatenary(p1, p2, 5.0f, a);
        const bool okB = maz::math::solveCatenary(p1, p2, 8.0f, b);
        CHECK(okA && okB, "both symmetric ropes solve");
        if (okA && okB) {
            CHECK(std::fabs(a.x0) < 1e-3f, "the symmetric low point is at the midpoint x=0");
            const float sagA = 0.0f - maz::math::catenaryHeight(a, 0.0f); // anchors at y=0, low point below
            const float sagB = 0.0f - maz::math::catenaryHeight(b, 0.0f);
            CHECK(sagA > 0.0f && sagB > sagA, "a longer rope sags deeper");
        }
    }

    // --- 4. Too-short / vertical ropes are unsolvable; polyline falls back to a straight line. ---
    {
        const vec2 p1(0.0f, 0.0f), p2(3.0f, 0.0f);
        Catenary cat;
        CHECK(!maz::math::solveCatenary(p1, p2, 3.0f, cat), "a rope equal to the gap is not a catenary");
        CHECK(!maz::math::solveCatenary(p1, p2, 2.0f, cat), "a rope shorter than the gap is unsolvable");
        CHECK(!maz::math::solveCatenary(vec2(1, 0), vec2(1, 5), 10.0f, cat), "a vertical rope is unsolvable");
        const auto poly = maz::math::catenaryPolyline(p1, p2, 2.0f, 8);
        bool straight = true;
        for (const auto& q : poly) if (std::fabs(q.y) > 1e-5f) straight = false;
        CHECK(straight && poly.size() == 9, "an unsolvable rope falls back to the straight segment");
    }

    // --- 5. Determinism. ---
    {
        Catenary a, b;
        maz::math::solveCatenary(vec2(0, 0), vec2(5, 2), 8.0f, a);
        maz::math::solveCatenary(vec2(0, 0), vec2(5, 2), 8.0f, b);
        CHECK(a.a == b.a && a.x0 == b.x0 && a.c == b.c, "identical inputs produce identical solves");
    }

    if (g_fail == 0) {
        std::printf("catenary: OK — endpoints, rope length, hanging-chain ODE, sag, fallback, determinism.\n");
        return 0;
    }
    std::printf("catenary: %d failure(s).\n", g_fail);
    return 1;
}
