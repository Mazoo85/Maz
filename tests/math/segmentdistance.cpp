// tests/math/segmentdistance.cpp — verifies closest-points-between-segments (math::closestBetweenSegments)
// and capsule overlap. Ground truths, hand-computed 3D configurations, deterministic:
//   * skew perpendicular segments have the exact gap and closest points (X-axis vs a raised Y-axis);
//   * crossing segments have distance 0 at the intersection;
//   * parallel offset segments have the perpendicular distance;
//   * collinear disjoint segments meet at their nearest endpoints;
//   * a zero-length segment reduces to point-vs-segment;
//   * parameters s,t stay in [0,1] and distance >= 0;
//   * capsulesOverlap agrees with distance vs (ra+rb).
#include "maz/math/SegmentDistance.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::SegmentClosest;
using maz::math::capsulesOverlap;
using maz::math::closestBetweenSegments;
using maz::math::vec3;

static bool vnear(const vec3& a, const vec3& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Skew perpendicular: A along X at origin, B along Y raised to z=1, centered at x=0.5. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(0, 0, 0), vec3(1, 0, 0), vec3(0.5f, -1, 1), vec3(0.5f, 1, 1));
        CHECK(near(r.distance, 1.0f), "skew perpendicular gap is 1");
        CHECK(vnear(r.pointA, vec3(0.5f, 0, 0)), "closest point on A is its midpoint");
        CHECK(vnear(r.pointB, vec3(0.5f, 0, 1)), "closest point on B is its midpoint");
        CHECK(near(r.s, 0.5f) && near(r.t, 0.5f), "both parameters are 0.5");
    }

    // --- 2. Crossing segments: distance 0. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(0, 0, 0), vec3(2, 0, 0), vec3(1, -1, 0), vec3(1, 1, 0));
        CHECK(near(r.distance, 0.0f), "crossing segments have zero distance");
        CHECK(vnear(r.pointA, vec3(1, 0, 0)) && vnear(r.pointB, vec3(1, 0, 0)),
              "they meet at the intersection point");
    }

    // --- 3. Parallel offset: perpendicular distance. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 2, 0), vec3(1, 2, 0));
        CHECK(near(r.distance, 2.0f), "parallel offset distance is the perpendicular gap");
    }

    // --- 4. Collinear disjoint: nearest endpoints. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(0, 0, 0), vec3(1, 0, 0), vec3(3, 0, 0), vec3(4, 0, 0));
        CHECK(near(r.distance, 2.0f), "collinear gap between nearest endpoints");
        CHECK(vnear(r.pointA, vec3(1, 0, 0)) && vnear(r.pointB, vec3(3, 0, 0)),
              "closest points are the facing endpoints");
    }

    // --- 5. Degenerate second segment (a point) reduces to point-vs-segment. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(0, 0, 0), vec3(2, 0, 0), vec3(1, 3, 0), vec3(1, 3, 0));
        CHECK(near(r.distance, 3.0f), "point above the middle of A: distance 3");
        CHECK(vnear(r.pointA, vec3(1, 0, 0)), "foot of perpendicular on A");
    }

    // --- 6. Parameters/range invariants over a few configs. ---
    {
        const SegmentClosest r = closestBetweenSegments(
            vec3(-1, -2, 3), vec3(4, 1, -2), vec3(0, 5, 5), vec3(2, -3, 1));
        CHECK(r.s >= 0.0f && r.s <= 1.0f && r.t >= 0.0f && r.t <= 1.0f, "s,t in [0,1]");
        CHECK(r.distance >= 0.0f, "distance non-negative");
    }

    // --- 7. Capsule overlap. ---
    {
        // Two parallel capsule spines 2 apart; radii 0.8 each -> gap 2 > 1.6, no overlap.
        CHECK(!capsulesOverlap(vec3(0, 0, 0), vec3(1, 0, 0), 0.8f, vec3(0, 2, 0), vec3(1, 2, 0), 0.8f),
              "far capsules do not overlap");
        // radii 1.1 each -> 2.2 > 2, overlap.
        CHECK(capsulesOverlap(vec3(0, 0, 0), vec3(1, 0, 0), 1.1f, vec3(0, 2, 0), vec3(1, 2, 0), 1.1f),
              "fat capsules overlap");
    }

    if (g_fail == 0) {
        std::printf("segmentdistance: OK — skew, crossing, parallel, collinear, degenerate, range, "
                    "capsules.\n");
        return 0;
    }
    std::printf("segmentdistance: %d failure(s).\n", g_fail);
    return 1;
}
