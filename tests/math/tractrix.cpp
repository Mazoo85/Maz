// tests/math/tractrix.cpp — verifies the tractrix "drag curve" (math Tractrix.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * CONSTANT LEASH (airtight, the defining property): the segment from the curve point to its drag point on
//     the x-axis has length exactly `a` at every parameter;
//   * LEASH IS TANGENT (airtight): that same leash segment is parallel to the curve's tangent — which is why
//     a towed object trails exactly along a tractrix;
//   * ARC LENGTH: the length from 0 to t is a·ln(cosh t), checked by an independent fine-polyline sum;
//   * SHAPE: starts at (0,a), y decreases monotonically toward 0, x increases; determinism.
#include "maz/math/Tractrix.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    const float a = 1.7f;

    // --- 1. Constant leash length = a (the defining property). ---
    {
        float worst = 0.0f;
        for (int i = 0; i < 400; ++i) {
            const float t = static_cast<float>(i) * 0.02f;
            const vec2 p = maz::math::tractrixPoint(a, t);
            const vec2 d = maz::math::tractrixDragPoint(a, t);
            worst = std::max(worst, std::fabs(len(p - d) - a));
        }
        CHECK(worst < 1e-4f, "the leash from the curve to its drag point is always length a");
    }

    // --- 2. Leash is parallel to the tangent (a towed object trails along the curve). ---
    {
        const float h = 1e-3f;
        float worstSin = 0.0f;
        for (int i = 5; i < 300; ++i) {
            const float t = static_cast<float>(i) * 0.02f;
            const vec2 p0 = maz::math::tractrixPoint(a, t - h);
            const vec2 p1 = maz::math::tractrixPoint(a, t + h);
            const vec2 tan = p1 - p0;                              // tangent direction
            const vec2 leash = maz::math::tractrixPoint(a, t) - maz::math::tractrixDragPoint(a, t);
            const float cross = tan.x * leash.y - tan.y * leash.x; // zero if parallel
            worstSin = std::max(worstSin, std::fabs(cross) / (len(tan) * len(leash)));
        }
        CHECK(worstSin < 2e-3f, "the leash is tangent to the curve at every point");
    }

    // --- 3. Arc length from 0 to T = a*ln(cosh T), via an independent fine-polyline sum. ---
    {
        const float T = 3.0f;
        const auto poly = maz::math::tractrixPolyline(a, 0.0f, T, 200000);
        float s = 0.0f;
        for (std::size_t i = 1; i < poly.size(); ++i) s += len(poly[i] - poly[i - 1]);
        CHECK(std::fabs(s - maz::math::tractrixArcLength(a, T)) < 2e-3f,
              "arc length equals a*ln(cosh t)");
    }

    // --- 4. Shape: starts at (0,a); y strictly decreases, x strictly increases. ---
    {
        const vec2 p0 = maz::math::tractrixPoint(a, 0.0f);
        CHECK(std::fabs(p0.x) < 1e-6f && std::fabs(p0.y - a) < 1e-6f, "t=0 starts at (0, a)");
        bool mono = true;
        vec2 prev = p0;
        for (int i = 1; i < 300; ++i) {
            const vec2 p = maz::math::tractrixPoint(a, static_cast<float>(i) * 0.02f);
            if (!(p.x > prev.x && p.y < prev.y)) mono = false;
            prev = p;
        }
        CHECK(mono, "x increases and y decreases monotonically as the object is dragged");
        CHECK(maz::math::tractrixPoint(a, 8.0f).y < 0.01f, "the object approaches the drag line");
    }

    // --- 5. Determinism + polyline endpoints. ---
    {
        const auto poly = maz::math::tractrixPolyline(a, 0.0f, 4.0f, 128);
        CHECK(poly.size() == 129, "the polyline has samples+1 points");
        CHECK(len(poly.back() - maz::math::tractrixPoint(a, 4.0f)) < 1e-5f, "ends at tEnd");
        CHECK(maz::math::tractrixPolyline(a, 0.0f, 4.0f, 64) == maz::math::tractrixPolyline(a, 0.0f, 4.0f, 64),
              "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("tractrix: OK — constant leash, tangent leash, arc length, shape, determinism.\n");
        return 0;
    }
    std::printf("tractrix: %d failure(s).\n", g_fail);
    return 1;
}
