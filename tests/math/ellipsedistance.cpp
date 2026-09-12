// tests/math/ellipsedistance.cpp — verifies closest point / distance to an axis-aligned ellipse
// (math EllipseDistance.hpp). Ground truths, deterministic (fixed + seeded-LCG points, no <random>, no clock):
//   * ON-BOUNDARY (airtight): the returned closest point always lies on the ellipse — (x/a)^2+(y/b)^2 == 1;
//   * PERPENDICULARITY (airtight): the vector p−closest is parallel to the ellipse normal (c.x/a^2, c.y/b^2)
//     at the foot — the defining optimality condition of a nearest point;
//   * BRUTE-FORCE ORACLE: the distance matches a dense boundary sampling refined by local bisection;
//   * SIGN: signedDistanceEllipse is negative strictly inside, positive strictly outside, ~0 on the boundary;
//   * CIRCLE special case (a==b): distance = | |p| − r |, closest point = r * p/|p|; axis-vertex cases.
#include "maz/math/EllipseDistance.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len2(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
};

// Brute-force nearest boundary point: coarse sample around the parametric ellipse, then bisect the angle.
static float bruteDist(float a, float b, const vec2& p) {
    const float TAU = 6.28318530718f;
    float bestT = 0.0f, bestD = 1e30f;
    const int N = 2048;
    for (int i = 0; i < N; ++i) {
        const float t = TAU * static_cast<float>(i) / static_cast<float>(N);
        const vec2 e(a * std::cos(t), b * std::sin(t));
        const float d = len2(vec2(p.x - e.x, p.y - e.y));
        if (d < bestD) {
            bestD = d;
            bestT = t;
        }
    }
    // Golden-section-ish refine around bestT.
    float lo = bestT - TAU / static_cast<float>(N), hi = bestT + TAU / static_cast<float>(N);
    for (int k = 0; k < 80; ++k) {
        const float m1 = lo + (hi - lo) / 3.0f, m2 = hi - (hi - lo) / 3.0f;
        const vec2 e1(a * std::cos(m1), b * std::sin(m1)), e2(a * std::cos(m2), b * std::sin(m2));
        if (len2(vec2(p.x - e1.x, p.y - e1.y)) < len2(vec2(p.x - e2.x, p.y - e2.y))) {
            hi = m2;
        } else {
            lo = m1;
        }
    }
    const float t = 0.5f * (lo + hi);
    const vec2 e(a * std::cos(t), b * std::sin(t));
    return len2(vec2(p.x - e.x, p.y - e.y));
}

int main() {
    using namespace maz::math;

    // --- 1. Boundary + perpendicularity + oracle over random points, several ellipses. ---
    {
        const float ells[3][2] = {{3.0f, 1.0f}, {1.0f, 4.0f}, {2.5f, 2.5f}};
        Lcg rng{0xE111u};
        float worstOn = 0.0f, worstPerp = 0.0f, worstOracle = 0.0f;
        int samples = 0;
        for (int e = 0; e < 3; ++e) {
            const float a = ells[e][0], b = ells[e][1];
            for (int i = 0; i < 3000; ++i) {
                const vec2 p(rng.sym() * 6.0f, rng.sym() * 6.0f);
                if (len2(p) < 1e-3f) {
                    continue; // skip the centre (closest point is non-unique)
                }
                const vec2 c = closestPointOnEllipse(a, b, p);
                // On the boundary.
                worstOn = std::max(worstOn, std::fabs((c.x / a) * (c.x / a) + (c.y / b) * (c.y / b) - 1.0f));
                // p - c parallel to the outward normal (c.x/a^2, c.y/b^2): cross product ~ 0 (normalized).
                const vec2 nrm(c.x / (a * a), c.y / (b * b));
                const vec2 d(p.x - c.x, p.y - c.y);
                const float nl = len2(nrm), dl = len2(d);
                if (nl > 1e-6f && dl > 1e-4f) {
                    const float cross = (d.x * nrm.y - d.y * nrm.x) / (nl * dl);
                    worstPerp = std::max(worstPerp, std::fabs(cross));
                }
                // Distance matches brute force.
                worstOracle = std::max(worstOracle, std::fabs(distanceToEllipse(a, b, p) - bruteDist(a, b, p)));
                ++samples;
            }
        }
        CHECK(samples > 8000, "the random points exercise many configurations");
        CHECK(worstOn < 2e-4f, "the closest point always lies on the ellipse");
        CHECK(worstPerp < 3e-3f, "p - closest is perpendicular to the ellipse (parallel to its normal)");
        CHECK(worstOracle < 3e-3f, "distanceToEllipse matches a dense brute-force boundary search");
    }

    // --- 2. Signed distance: sign matches inside/outside; ~0 on the boundary. ---
    {
        const float a = 2.0f, b = 1.0f;
        CHECK(signedDistanceEllipse(a, b, vec2(0, 0)) < 0.0f, "the centre is inside (negative)");
        CHECK(signedDistanceEllipse(a, b, vec2(1.0f, 0)) < 0.0f, "an interior point is negative");
        CHECK(signedDistanceEllipse(a, b, vec2(5.0f, 0)) > 0.0f, "an exterior point is positive");
        CHECK(std::fabs(signedDistanceEllipse(a, b, vec2(a, 0))) < 1e-4f, "the +x vertex is on the boundary");
        CHECK(std::fabs(signedDistanceEllipse(a, b, vec2(0, b))) < 1e-4f, "the +y vertex is on the boundary");
        // Just inside / just outside along a diagonal boundary point.
        const vec2 bp = closestPointOnEllipse(a, b, vec2(3.0f, 3.0f)); // a boundary point
        const vec2 outN(bp.x / (a * a), bp.y / (b * b));
        const float on = len2(outN);
        const vec2 unit(outN.x / on, outN.y / on);
        CHECK(signedDistanceEllipse(a, b, vec2(bp.x + unit.x * 0.1f, bp.y + unit.y * 0.1f)) > 0.0f,
              "stepping out along the normal is positive");
        CHECK(signedDistanceEllipse(a, b, vec2(bp.x - unit.x * 0.1f, bp.y - unit.y * 0.1f)) < 0.0f,
              "stepping in along the normal is negative");
    }

    // --- 3. Circle special case (a == b == r): closed form. ---
    {
        const float r = 2.0f;
        // Outside.
        CHECK(std::fabs(distanceToEllipse(r, r, vec2(5, 0)) - 3.0f) < 1e-4f, "circle outside distance is |p|-r");
        const vec2 c = closestPointOnEllipse(r, r, vec2(3.0f, 4.0f)); // |p|=5
        CHECK(len2(vec2(c.x - 1.2f, c.y - 1.6f)) < 1e-3f, "circle closest point is r * p/|p|");
        // Inside.
        CHECK(std::fabs(distanceToEllipse(r, r, vec2(0.5f, 0)) - 1.5f) < 1e-4f, "circle inside distance is r-|p|");
    }

    if (g_fail == 0) {
        std::printf("ellipsedistance: OK — on-boundary, perpendicularity, oracle, sign, circle.\n");
        return 0;
    }
    std::printf("ellipsedistance: %d failure(s).\n", g_fail);
    return 1;
}
