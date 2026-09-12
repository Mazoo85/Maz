// tests/math/minkowskisum.cpp — verifies the Minkowski sum of two convex polygons (math MinkowskiSum.hpp).
// Ground truths, deterministic (seeded LCG for random shapes/directions, no <random>, no clock):
//   * SUPPORT IDENTITY (airtight analytic oracle): for every direction d, the extent of the sum equals the
//     sum of the two inputs' extents — support(A+B, d) == support(A, d) + support(B, d);
//   * a unit square (+) a unit square is the 2x2 square (area 4);
//   * the result is convex (all boundary turns the same way);
//   * BRUTE CONTAINMENT: for random interior points a in A and b in B, a+b lies inside the sum polygon;
//   * determinism.
#include "maz/math/MinkowskiSum.hpp"
#include "maz/math/Geometry2D.hpp" // polygonArea, pointInPolygon, convexHull

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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

static float support(const std::vector<vec2>& poly, float dx, float dy) {
    float best = -1e30f;
    for (const vec2& v : poly) {
        const float d = v.x * dx + v.y * dy;
        if (d > best) best = d;
    }
    return best;
}

static bool isConvexCcw(const std::vector<vec2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) return false;
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& a = poly[i];
        const vec2& b = poly[(i + 1) % n];
        const vec2& c = poly[(i + 2) % n];
        const float cr = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cr < -1e-3f) return false; // a clockwise turn -> not convex CCW
    }
    return true;
}

int main() {
    // --- 1. Unit square (+) unit square == 2x2 square. ---
    {
        std::vector<vec2> sq{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        const std::vector<vec2> sum = maz::math::minkowskiSumConvex(sq, sq);
        CHECK(std::fabs(maz::math::polygonArea(sum) - 4.0f) < 1e-3f, "unit square (+) unit square has area 4");
        // Extent is [0,2] on both axes.
        CHECK(std::fabs(support(sum, 1, 0) - 2.0f) < 1e-4f && std::fabs(support(sum, 0, 1) - 2.0f) < 1e-4f,
              "unit-square sum spans [0,2] on each axis");
        CHECK(isConvexCcw(sum), "the sum of two squares is convex CCW");
    }

    // --- 2. Support identity + convexity over random convex shapes. ---
    {
        Lcg rng{0x11CE5u};
        bool identOk = true, convexOk = true;
        int trials = 0;
        for (int t = 0; t < 60; ++t) {
            std::vector<vec2> A, B;
            const int na = 4 + static_cast<int>(rng.next() % 6u);
            const int nb = 4 + static_cast<int>(rng.next() % 6u);
            for (int i = 0; i < na; ++i) A.push_back(vec2(rng.range(-3, 3), rng.range(-3, 3)));
            for (int i = 0; i < nb; ++i) B.push_back(vec2(rng.range(-2, 2) + 5.0f, rng.range(-2, 2)));
            const std::vector<vec2> hullA = maz::math::convexHull(A);
            const std::vector<vec2> hullB = maz::math::convexHull(B);
            if (hullA.size() < 3 || hullB.size() < 3) continue;
            const std::vector<vec2> sum = maz::math::minkowskiSumConvex(A, B);
            if (!isConvexCcw(sum)) convexOk = false;
            // support(A+B, d) == support(A, d) + support(B, d) for many directions.
            for (int k = 0; k < 40; ++k) {
                const float ang = rng.range(0.0f, 6.2831853f);
                const float dx = std::cos(ang), dy = std::sin(ang);
                const float lhs = support(sum, dx, dy);
                const float rhs = support(hullA, dx, dy) + support(hullB, dx, dy);
                if (std::fabs(lhs - rhs) > 1e-2f) identOk = false;
            }
            ++trials;
        }
        CHECK(trials > 40, "generated enough valid random shape pairs");
        CHECK(identOk, "support(A+B, d) == support(A, d) + support(B, d) for all directions");
        CHECK(convexOk, "every random Minkowski sum is convex CCW");
    }

    // --- 3. Brute-force containment: interior a + interior b is inside the sum. ---
    {
        Lcg rng{0x7A9Bu};
        std::vector<vec2> A{{-1, -1}, {2, -1}, {1, 2}, {-1, 1}};      // convex quad
        std::vector<vec2> B{{0, 0}, {1.5f, 0}, {1.2f, 1.3f}, {0, 1}}; // convex quad
        const std::vector<vec2> hullA = maz::math::convexHull(A);
        const std::vector<vec2> hullB = maz::math::convexHull(B);
        const std::vector<vec2> sum = maz::math::minkowskiSumConvex(A, B);
        bool allInside = true;
        auto interior = [&](const std::vector<vec2>& h) {
            // Random convex combination -> strictly interior point.
            float w[16], tot = 0.0f;
            for (std::size_t i = 0; i < h.size(); ++i) { w[i] = rng.range(0.05f, 1.0f); tot += w[i]; }
            vec2 p{0, 0};
            for (std::size_t i = 0; i < h.size(); ++i) { p.x += w[i] / tot * h[i].x; p.y += w[i] / tot * h[i].y; }
            return p;
        };
        for (int t = 0; t < 400; ++t) {
            const vec2 a = interior(hullA), b = interior(hullB);
            if (!maz::math::pointInPolygon(vec2(a.x + b.x, a.y + b.y), sum)) allInside = false;
        }
        CHECK(allInside, "interior a + interior b always lies inside the Minkowski sum");
    }

    // --- 4. Determinism. ---
    {
        std::vector<vec2> A{{0, 0}, {2, 0}, {1, 2}};
        std::vector<vec2> B{{0, 0}, {1, 0}, {0, 1}};
        const std::vector<vec2> a = maz::math::minkowskiSumConvex(A, B);
        const std::vector<vec2> b = maz::math::minkowskiSumConvex(A, B);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i].x != b[i].x || a[i].y != b[i].y) same = false;
        CHECK(same, "identical inputs produce identical sums");
    }

    if (g_fail == 0) {
        std::printf("minkowskisum: OK — square sum, support identity, convexity, containment, determinism.\n");
        return 0;
    }
    std::printf("minkowskisum: %d failure(s).\n", g_fail);
    return 1;
}
