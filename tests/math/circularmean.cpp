// tests/math/circularmean.cpp — verifies circular statistics (math CircularMean.hpp).
// Ground truths, deterministic:
//   * the mean of 350deg and 10deg is 0deg (the case naive averaging gets wrong, giving 180deg);
//   * symmetric pairs average to their bisector; identical angles average to themselves;
//   * resultant length R is 1 for identical angles, ~0 for four evenly-spread angles;
//   * circular variance = 1 - R; std dev is 0 for identical angles and grows as spread grows;
//   * a weighted mean leans toward the heavier angle;
//   * results are wrapped into (-pi, pi].
#include "maz/math/CircularMean.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::circularMean;
using maz::math::circularMeanWeighted;
using maz::math::circularStdDev;
using maz::math::circularVariance;
using maz::math::resultantLength;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static const float PI = 3.14159265358979323846f;
static float deg(float d) { return d * PI / 180.0f; }
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
// Compare two angles on the circle (shortest difference).
static bool nearAngle(float a, float b, float e = 1e-3f) {
    float d = std::fabs(a - b);
    while (d > PI) d = std::fabs(d - 2.0f * PI);
    return d < e;
}

int main() {
    // --- 1. The wrap-around case. ---
    {
        const std::vector<float> a{deg(350.0f), deg(10.0f)};
        CHECK(nearAngle(circularMean(a), 0.0f), "mean of 350 and 10 degrees is 0 (not 180)");
        // Naive arithmetic mean would be 180deg — confirm we are NOT that.
        CHECK(!nearAngle(circularMean(a), PI), "circular mean avoids the naive 180-degree answer");
    }

    // --- 2. Symmetric + identical. ---
    {
        CHECK(nearAngle(circularMean({deg(80.0f), deg(100.0f)}), deg(90.0f)), "80 & 100 -> 90");
        CHECK(nearAngle(circularMean({deg(45.0f), deg(45.0f), deg(45.0f)}), deg(45.0f)),
              "identical angles average to themselves");
        // A symmetric spread about pi should mean to pi (or -pi, same point).
        CHECK(nearAngle(circularMean({deg(170.0f), deg(190.0f)}), PI), "170 & 190 -> 180");
    }

    // --- 3. Resultant length + variance + std dev. ---
    {
        const std::vector<float> same{deg(30.0f), deg(30.0f), deg(30.0f)};
        CHECK(near(resultantLength(same), 1.0f), "identical angles -> R = 1");
        CHECK(near(circularVariance(same), 0.0f), "identical angles -> variance 0");
        CHECK(near(circularStdDev(same), 0.0f), "identical angles -> std dev 0");

        const std::vector<float> spread{deg(0.0f), deg(90.0f), deg(180.0f), deg(270.0f)};
        CHECK(resultantLength(spread) < 1e-4f, "four evenly-spread angles -> R ~ 0");
        CHECK(near(circularVariance(spread), 1.0f, 1e-3f), "evenly spread -> variance ~ 1");

        // More spread -> larger std dev.
        const std::vector<float> tight{deg(-5.0f), deg(0.0f), deg(5.0f)};
        const std::vector<float> wide{deg(-60.0f), deg(0.0f), deg(60.0f)};
        CHECK(circularStdDev(wide) > circularStdDev(tight), "wider spread gives a larger circular std dev");
    }

    // --- 4. Weighted mean leans toward the heavier angle. ---
    {
        const std::vector<float> a{deg(0.0f), deg(90.0f)};
        const std::vector<float> w{9.0f, 1.0f};
        const float m = circularMeanWeighted(a, w);
        CHECK(m > deg(0.0f) && m < deg(45.0f), "9:1 weighting pulls the mean toward 0 (below the midpoint)");
        // Equal weights reproduce the unweighted mean.
        CHECK(nearAngle(circularMeanWeighted(a, {1.0f, 1.0f}), circularMean(a)),
              "equal weights match the unweighted mean");
    }

    // --- 5. Range + degenerate inputs. ---
    {
        // A near-pi cluster should stay within (-pi, pi].
        const float m = circularMean({deg(179.0f), deg(-179.0f)});
        CHECK(m >= -PI - 1e-4f && m <= PI + 1e-4f, "result is within (-pi, pi]");
        CHECK(nearAngle(m, PI), "179 & -179 average to 180 (the far side)");
        CHECK(near(circularMean({}), 0.0f), "empty set -> 0");
        CHECK(near(resultantLength({}), 0.0f), "empty set -> R 0");
    }

    if (g_fail == 0) {
        std::printf("circularmean: OK — wrap case, symmetric, R/variance/stddev, weighted, range.\n");
        return 0;
    }
    std::printf("circularmean: %d failure(s).\n", g_fail);
    return 1;
}
