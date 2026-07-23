// tests/math/closestpointcurve.cpp — verifies point-onto-path projection (math ClosestPointCurve.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * a point already ON the path projects to itself (zero distance);
//   * OPTIMALITY: the returned distance is <= the distance to any of a fine set of sample points on the path
//     (brute-force lower-bound cross-check);
//   * ANALYTIC: a point a known perpendicular offset from a straight segment reports exactly that distance,
//     at the expected foot of the perpendicular;
//   * a point past an endpoint clamps to the endpoint;
//   * PERPENDICULARITY: for an interior projection the residual is perpendicular to the local path tangent;
//   * determinism.
#include "maz/math/ClosestPointCurve.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::Curve2D;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

static float dist(const vec2& a, const vec2& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    Lcg rng{0x0C0Fu};

    // --- 1. Polyline: analytic offset + optimality + on-path + endpoint clamp. ---
    {
        std::vector<vec2> poly{{0, 0}, {10, 0}, {10, 10}, {20, 10}};

        // Analytic: (4, 3) is 3 units above the first horizontal segment; foot at (4,0).
        {
            const auto pr = maz::math::closestPointOnPolyline(poly, vec2(4, 3));
            CHECK(std::fabs(pr.distance - 3.0f) < 1e-4f, "perpendicular offset from a segment is exact");
            CHECK(dist(pr.point, vec2(4, 0)) < 1e-4f, "the foot of the perpendicular is correct");
            CHECK(std::fabs(pr.offset - 0.4f) < 1e-4f, "the along-path offset is correct");
        }
        // On-path point projects to itself.
        {
            const vec2 on = vec2(10, 4); // on the 2nd segment
            const auto pr = maz::math::closestPointOnPolyline(poly, on);
            CHECK(pr.distance < 1e-4f && dist(pr.point, on) < 1e-4f, "a point on the path projects to itself");
        }
        // Past the far endpoint clamps to it.
        {
            const auto pr = maz::math::closestPointOnPolyline(poly, vec2(30, 10));
            CHECK(dist(pr.point, vec2(20, 10)) < 1e-4f, "a point past the end clamps to the endpoint");
        }
        // Optimality vs a fine brute-force sampling of the polyline.
        {
            bool ok = true;
            for (int trial = 0; trial < 300; ++trial) {
                const vec2 q(rng.range(-5, 25), rng.range(-5, 15));
                const auto pr = maz::math::closestPointOnPolyline(poly, q);
                float brute = 1e30f;
                for (std::size_t i = 0; i + 1 < poly.size(); ++i)
                    for (int s = 0; s <= 200; ++s) {
                        const float t = static_cast<float>(s) / 200.0f;
                        const vec2 sp(poly[i].x + (poly[i + 1].x - poly[i].x) * t,
                                      poly[i].y + (poly[i + 1].y - poly[i].y) * t);
                        brute = std::min(brute, dist(q, sp));
                    }
                if (pr.distance > brute + 1e-3f) ok = false;
            }
            CHECK(ok, "polyline projection is <= the distance to every sampled path point");
        }
    }

    // --- 2. Curve2D: on-curve projects to itself + optimality + perpendicular residual. ---
    {
        Curve2D curve;
        curve.addPoint(vec2(0, 0), vec2(0, 0), vec2(3, 0));
        curve.addPoint(vec2(10, 8), vec2(-3, 0), vec2(3, 0));
        curve.addPoint(vec2(20, 0), vec2(-3, 0), vec2(0, 0));

        // A point sampled directly on the curve should project back with ~zero distance.
        {
            bool ok = true;
            for (int i = 0; i < 20; ++i) {
                const float f = rng.range(0.0f, 2.0f);
                const vec2 on = curve.sample(f);
                const auto pr = maz::math::closestPointOnCurve(curve, on, 24);
                if (pr.distance > 5e-2f) ok = false;
            }
            CHECK(ok, "a point on the curve projects back onto it (near-zero distance)");
        }
        // Optimality vs a very fine brute-force sampling of the curve.
        {
            bool ok = true;
            for (int trial = 0; trial < 120; ++trial) {
                const vec2 q(rng.range(-5, 25), rng.range(-8, 14));
                const auto pr = maz::math::closestPointOnCurve(curve, q, 24);
                float brute = 1e30f;
                for (int s = 0; s <= 4000; ++s) {
                    const float f = 2.0f * static_cast<float>(s) / 4000.0f;
                    brute = std::min(brute, dist(q, curve.sample(f)));
                }
                if (pr.distance > brute + 2e-2f) ok = false;
            }
            CHECK(ok, "curve projection is <= the distance to every finely-sampled curve point");
        }
        // Perpendicularity: residual is orthogonal to the tangent at an interior projection.
        {
            const vec2 q(5, -6); // off to one side, projects to the interior
            const auto pr = maz::math::closestPointOnCurve(curve, q, 24);
            if (pr.offset > 0.05f && pr.offset < 1.95f) {
                const vec2 tan = curve.tangent(pr.offset);
                const vec2 res(q.x - pr.point.x, q.y - pr.point.y);
                const float rl = std::sqrt(res.x * res.x + res.y * res.y);
                const float align = rl > 1e-4f ? (res.x * tan.x + res.y * tan.y) / rl : 0.0f;
                CHECK(std::fabs(align) < 5e-2f, "the residual is perpendicular to the curve tangent");
            }
        }
    }

    // --- 3. Determinism. ---
    {
        std::vector<vec2> poly{{0, 0}, {5, 2}, {9, -1}, {12, 4}};
        const auto a = maz::math::closestPointOnPolyline(poly, vec2(6, 6));
        const auto b = maz::math::closestPointOnPolyline(poly, vec2(6, 6));
        CHECK(a.point.x == b.point.x && a.point.y == b.point.y && a.offset == b.offset,
              "identical inputs produce identical projections");
    }

    if (g_fail == 0) {
        std::printf("closestpointcurve: OK — analytic, optimality, on-path, clamp, perpendicular, determinism.\n");
        return 0;
    }
    std::printf("closestpointcurve: %d failure(s).\n", g_fail);
    return 1;
}
