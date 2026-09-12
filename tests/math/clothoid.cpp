// tests/math/clothoid.cpp — verifies the clothoid / Euler spiral (math Clothoid.hpp).
// Ground truths, deterministic (fixed parameters, no <random>, no clock):
//   * LINEAR CURVATURE (the defining property): the curvature measured geometrically from the sampled curve
//     (Menger / circumradius of three consecutive points) equals κ0 + rate·s all along the spiral;
//   * DEGENERATE CASES: rate = 0, κ0 = 0 is an exactly straight line; rate = 0, κ0 = k is an exact circle of
//     radius 1/k (all points equidistant from the centre);
//   * UNIT SPEED: consecutive samples are spaced by the requested arc-length step (the curve is
//     arc-length-parameterised);
//   * the single-point evaluator agrees with the polyline endpoint; heading/curvature helpers are consistent;
//   * determinism.
#include "maz/math/Clothoid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// Unsigned curvature of the circle through three points (Menger curvature).
static float menger(const vec2& a, const vec2& b, const vec2& c) {
    const float ab = len(b - a), bc = len(c - b), ca = len(a - c);
    const float area = std::fabs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
    return 4.0f * area / (ab * bc * ca);
}

int main() {
    // --- 1. Linear curvature along the spiral. ---
    {
        const float k0 = 0.1f, rate = 0.05f, L = 10.0f;
        const int segs = 400;
        const auto poly = maz::math::clothoidPolyline(vec2(0, 0), 0.0f, k0, rate, L, segs);
        const float ds = L / static_cast<float>(segs);
        float worst = 0.0f;
        for (int i = 20; i < segs - 20; i += 20) {
            const float kEst = menger(poly[static_cast<std::size_t>(i - 1)], poly[static_cast<std::size_t>(i)],
                                      poly[static_cast<std::size_t>(i + 1)]);
            const float s = static_cast<float>(i) * ds;
            const float kTrue = std::fabs(maz::math::clothoidCurvature(k0, rate, s));
            worst = std::max(worst, std::fabs(kEst - kTrue));
        }
        CHECK(worst < 5e-3f, "the measured curvature equals kappa0 + rate*s all along the spiral");
    }

    // --- 2. Straight line when kappa0 = 0 and rate = 0. ---
    {
        const float th = 0.3f;
        const auto line = maz::math::clothoidPolyline(vec2(1, 2), th, 0.0f, 0.0f, 5.0f, 50);
        const vec2 dir(std::cos(th), std::sin(th));
        float maxDev = 0.0f;
        for (const vec2& p : line) {
            const vec2 rel = p - vec2(1, 2);
            maxDev = std::max(maxDev, std::fabs(rel.x * dir.y - rel.y * dir.x)); // perpendicular offset
        }
        CHECK(maxDev < 1e-4f, "a zero-curvature clothoid is an exactly straight line along its heading");
        // And the far endpoint is length units away along the heading.
        CHECK(std::fabs(len(line.back() - vec2(1, 2)) - 5.0f) < 1e-3f, "the straight clothoid has the right length");
    }

    // --- 3. Circular arc when rate = 0, kappa0 != 0: radius = 1/kappa0. ---
    {
        const float kc = 0.5f; // radius 2
        const auto circ = maz::math::clothoidPolyline(vec2(0, 0), 0.0f, kc, 0.0f, 6.0f, 300);
        // Starting at origin heading +x with left curvature kc, the centre is at (0, 1/kc).
        const vec2 center(0.0f, 1.0f / kc);
        const float R = 1.0f / kc;
        float maxRErr = 0.0f;
        for (const vec2& p : circ) {
            maxRErr = std::max(maxRErr, std::fabs(len(p - center) - R));
        }
        CHECK(maxRErr < 1e-3f, "a constant-curvature clothoid is an exact circle of radius 1/kappa0");
    }

    // --- 4. Unit-speed spacing. ---
    {
        const float L = 8.0f;
        const int segs = 200;
        const auto poly = maz::math::clothoidPolyline(vec2(0, 0), 0.2f, 0.15f, 0.03f, L, segs);
        const float ds = L / static_cast<float>(segs);
        float worst = 0.0f;
        for (std::size_t i = 1; i < poly.size(); ++i) {
            worst = std::max(worst, std::fabs(len(poly[i] - poly[i - 1]) - ds));
        }
        CHECK(worst < ds * 0.02f, "consecutive samples are one arc-length step apart (unit speed)");
    }

    // --- 5. Single-point evaluator matches the polyline endpoint; heading helper consistent. ---
    {
        const float k0 = 0.1f, rate = 0.05f, L = 10.0f;
        const auto poly = maz::math::clothoidPolyline(vec2(0, 0), 0.0f, k0, rate, L, 400);
        const vec2 sp = maz::math::clothoidPoint(vec2(0, 0), 0.0f, k0, rate, L);
        CHECK(len(sp - poly.back()) < 2e-3f, "clothoidPoint agrees with the polyline endpoint");
        CHECK(std::fabs(maz::math::clothoidHeading(0.0f, k0, rate, L) - (k0 * L + 0.5f * rate * L * L)) < 1e-5f,
              "clothoidHeading returns theta0 + kappa0*s + rate*s^2/2");
    }

    // --- 6. Determinism. ---
    {
        const auto a = maz::math::clothoidPolyline(vec2(0, 0), 0.1f, 0.2f, 0.05f, 4.0f, 60);
        const auto b = maz::math::clothoidPolyline(vec2(0, 0), 0.1f, 0.2f, 0.05f, 4.0f, 60);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i) same = a[i].x == b[i].x && a[i].y == b[i].y;
        CHECK(same, "identical inputs produce identical polylines");
    }

    if (g_fail == 0) {
        std::printf("clothoid: OK — linear curvature, straight line, circle, unit speed, point/heading, "
                    "determinism.\n");
        return 0;
    }
    std::printf("clothoid: %d failure(s).\n", g_fail);
    return 1;
}
