// tests/math/arclength.cpp — verifies arc-length reparameterization (math::ArcLengthTable).
// Ground truths, exact/continuous math, deterministic:
//   * total length is the summed chord length;
//   * on a uniformly-sampled straight line, distance maps linearly to parameter (u = d/length);
//   * the distance<->parameter round-trip is exact (distanceAtParameter(parameterAtDistance(d)) == d);
//   * on a NON-uniform-speed sampling, arc-length param differs from raw param (the whole point):
//     the midpoint by distance is NOT the midpoint by parameter;
//   * clamping past the ends returns 0 and 1;
//   * equalArcParameters yields points spaced at equal arc length.
#include "maz/math/ArcLength.hpp"
#include "maz/math/CatmullRomSpline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::ArcLengthTable;
using maz::math::vec2;

static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Straight line, uniform sampling: distance maps linearly to parameter. ---
    {
        const std::vector<vec2> pts = {vec2(0, 0), vec2(1, 0), vec2(2, 0), vec2(3, 0)};
        ArcLengthTable t(pts);
        CHECK(near(t.totalLength(), 3.0f), "total length is 3");
        CHECK(near(t.parameterAtDistance(0.0f), 0.0f), "distance 0 -> u 0");
        CHECK(near(t.parameterAtDistance(3.0f), 1.0f), "distance 3 -> u 1");
        CHECK(near(t.parameterAtDistance(1.5f), 0.5f), "half distance -> u 0.5 on a straight line");
        CHECK(near(t.parameterAtFraction(0.5f), 0.5f), "fraction 0.5 -> u 0.5");
    }

    // --- 2. Round-trip distance <-> parameter. ---
    {
        const std::vector<vec2> pts = {vec2(0, 0), vec2(2, 1), vec2(3, 4), vec2(6, 5), vec2(7, 0)};
        ArcLengthTable t(pts);
        for (int k = 0; k <= 10; ++k) {
            const float d = t.totalLength() * static_cast<float>(k) / 10.0f;
            const float u = t.parameterAtDistance(d);
            const float back = t.distanceAtParameter(u);
            CHECK(near(back, d, 1e-2f), "distance -> u -> distance round-trips");
        }
    }

    // --- 3. Non-uniform speed: arc-length param differs from raw param. ---
    {
        // Points bunched near the start, then a long jump: raw u midpoint (0.5) is far from the
        // distance midpoint.
        const std::vector<vec2> pts = {vec2(0, 0), vec2(0.1f, 0), vec2(0.2f, 0), vec2(3.0f, 0)};
        ArcLengthTable t(pts);
        CHECK(near(t.totalLength(), 3.0f), "length 3 (0.1+0.1+2.8)");
        const float uHalf = t.parameterAtFraction(0.5f); // u at half the distance (1.5)
        // Half distance (1.5) lands well inside the last long segment, so u > 2/3.
        CHECK(uHalf > 0.66f, "distance-midpoint parameter is past the raw midpoint (non-uniform)");
        CHECK(std::fabs(uHalf - 0.5f) > 0.1f, "arc-length param differs from raw param");
    }

    // --- 4. Clamping. ---
    {
        const std::vector<vec2> pts = {vec2(0, 0), vec2(1, 0), vec2(2, 0)};
        ArcLengthTable t(pts);
        CHECK(t.parameterAtDistance(-5.0f) == 0.0f, "negative distance clamps to 0");
        CHECK(t.parameterAtDistance(100.0f) == 1.0f, "over-length distance clamps to 1");
    }

    // --- 5. Equal-arc resampling of a real spline: consecutive points equidistant. ---
    {
        maz::math::CatmullRomSpline sp;
        sp.points = {vec2(0, 0), vec2(1, 3), vec2(4, 3), vec2(5, 0), vec2(8, 1)};
        const std::vector<vec2> dense = sp.tessellate(40);
        ArcLengthTable t(dense);
        const std::vector<float> params = t.equalArcParameters(9); // 8 equal-length spans
        // Convert each param back to a point and check span lengths are ~equal.
        std::vector<vec2> ep;
        for (float u : params) {
            ep.push_back(sp.eval(u * static_cast<float>(sp.points.size() - 1)));
        }
        float minSpan = 1e9f, maxSpan = 0.0f;
        for (std::size_t i = 1; i < ep.size(); ++i) {
            const vec2 d = ep[i] - ep[i - 1];
            const float len = std::sqrt(d.x * d.x + d.y * d.y);
            minSpan = std::min(minSpan, len);
            maxSpan = std::max(maxSpan, len);
        }
        // Equal-arc spacing: spans should be close (within ~15% given discrete tessellation).
        CHECK(maxSpan > 0.0f && (maxSpan - minSpan) / maxSpan < 0.15f,
              "equalArcParameters gives roughly equidistant points along the spline");
    }

    if (g_fail == 0) {
        std::printf("arclength: OK — total length, linear straight-line map, round-trip, non-uniform, "
                    "clamping, equal-arc resample.\n");
        return 0;
    }
    std::printf("arclength: %d failure(s).\n", g_fail);
    return 1;
}
