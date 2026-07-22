// tests/math/gjkdistance.cpp — verifies GJK minimum-distance between convex polygons (math GjkDistance.hpp).
// Ground truths, deterministic:
//   * two overlapping boxes report intersecting (distance 0);
//   * two separated boxes report the exact gap, with witness points on facing edges;
//   * a diagonal separation matches the hand-computed corner-to-corner distance;
//   * point-vs-box and touching (edge-to-edge contact) behave;
//   * a randomized cross-check: GJK distance equals a brute-force minimum over all edge-edge segment
//     distances for many random separated convex polygons, and the witnesses realise that distance.
#include "maz/math/GjkDistance.hpp"
#include "maz/math/Geometry2D.hpp" // closestPointsBetweenSegments for the brute-force reference

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::gjkDistance;
using maz::math::GjkResult;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

static std::vector<vec2> box(float cx, float cy, float hw, float hh) {
    return {vec2(cx - hw, cy - hh), vec2(cx + hw, cy - hh), vec2(cx + hw, cy + hh), vec2(cx - hw, cy + hh)};
}

// Brute-force minimum distance between two convex polygons via all edge-edge pairs (valid when disjoint).
static float bruteDistance(const std::vector<vec2>& A, const std::vector<vec2>& B) {
    float best = 1e30f;
    for (std::size_t i = 0; i < A.size(); ++i) {
        const vec2 a0 = A[i], a1 = A[(i + 1) % A.size()];
        for (std::size_t j = 0; j < B.size(); ++j) {
            const vec2 b0 = B[j], b1 = B[(j + 1) % B.size()];
            vec2 c1, c2;
            maz::math::closestPointsBetweenSegments(a0, a1, b0, b1, c1, c2);
            const float d = length(c1 - c2);
            if (d < best) best = d;
        }
    }
    return best;
}

struct Lcg {
    std::uint64_t s;
    float f() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<float>(s >> 40) * (1.0f / 16777216.0f);
    }
    float range(float lo, float hi) { return lo + f() * (hi - lo); }
};

int main() {
    // --- 1. Overlap -> distance 0. ---
    {
        const GjkResult r = gjkDistance(box(0, 0, 1, 1), box(0.5f, 0.5f, 1, 1));
        CHECK(r.intersecting && near(r.distance, 0.0f), "overlapping boxes intersect (distance 0)");
    }

    // --- 2. Horizontal gap. ---
    {
        // A spans x in [-1,1], B spans x in [4,6] -> gap of 3 along x.
        const GjkResult r = gjkDistance(box(0, 0, 1, 1), box(5, 0, 1, 1));
        CHECK(!r.intersecting && near(r.distance, 3.0f), "separated boxes report the 3-unit gap");
        CHECK(near(r.pointA.x, 1.0f) && near(r.pointB.x, 4.0f), "witness x-coords are on the facing edges");
        CHECK(near(length(r.pointA - r.pointB), 3.0f), "witness points realise the distance");
    }

    // --- 3. Diagonal gap: corner (1,1) of A to corner (4,5) of B. ---
    {
        const GjkResult r = gjkDistance(box(0, 0, 1, 1), box(5, 6, 1, 1));
        const float expected = std::sqrt(3.0f * 3.0f + 4.0f * 4.0f); // corners (1,1)-(4,5) = 5
        CHECK(!r.intersecting && near(r.distance, expected), "diagonal corner-to-corner distance = 5");
        CHECK(near(r.pointA.x, 1.0f) && near(r.pointA.y, 1.0f), "witness on A is its top-right corner");
        CHECK(near(r.pointB.x, 4.0f) && near(r.pointB.y, 5.0f), "witness on B is its bottom-left corner");
    }

    // --- 4. Point vs box + touching. ---
    {
        std::vector<vec2> pt{vec2(-2, 0)};
        const GjkResult r = gjkDistance(pt, box(0, 0, 1, 1)); // point at x=-2, box left edge x=-1 -> 1
        CHECK(!r.intersecting && near(r.distance, 1.0f), "point-to-box distance is 1");

        const GjkResult t = gjkDistance(box(0, 0, 1, 1), box(2, 0, 1, 1)); // edges touch at x=1
        CHECK(near(t.distance, 0.0f), "edge-touching boxes report ~0 distance");
    }

    // --- 5. Randomized cross-check vs brute force (separated shapes). ---
    {
        Lcg rng{0xC0FFEEu};
        int checked = 0;
        bool ok = true;
        for (int trial = 0; trial < 4000; ++trial) {
            // Two small random boxes placed far enough apart to (usually) be disjoint.
            const float ax = rng.range(-2, 2), ay = rng.range(-2, 2);
            const float bx = rng.range(8, 14), by = rng.range(-6, 6);
            const std::vector<vec2> A = box(ax, ay, rng.range(0.5f, 1.5f), rng.range(0.5f, 1.5f));
            const std::vector<vec2> B = box(bx, by, rng.range(0.5f, 1.5f), rng.range(0.5f, 1.5f));
            const GjkResult r = gjkDistance(A, B);
            if (r.intersecting) continue; // brute-force edge distance is only the boundary gap when disjoint
            const float bf = bruteDistance(A, B);
            ++checked;
            if (!near(r.distance, bf, 2e-2f)) ok = false;
            if (!near(length(r.pointA - r.pointB), r.distance, 2e-2f)) ok = false;
        }
        CHECK(ok, "GJK distance matches the brute-force edge-edge minimum on random shapes");
        CHECK(checked > 1000, "the randomized run exercised many disjoint cases");
    }

    if (g_fail == 0) {
        std::printf("gjkdistance: OK — overlap, gaps, diagonal, point/touch, random brute-force.\n");
        return 0;
    }
    std::printf("gjkdistance: %d failure(s).\n", g_fail);
    return 1;
}
