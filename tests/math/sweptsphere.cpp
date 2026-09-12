// tests/math/sweptsphere.cpp — verifies swept sphere vs plane (math::sweepSpherePlane).
// Ground truths, hand-computed impact times, deterministic:
//   * a sphere falling toward the ground first touches at t = (dist - r)/speed, with the contact point
//     on the plane;
//   * a sphere already overlapping the plane reports t = 0;
//   * a sphere moving AWAY from the plane does not hit;
//   * a sphere too slow to reach the plane within the step does not hit (t would exceed 1);
//   * approaching the plane from the other side works symmetrically;
//   * a sphere moving parallel to the plane does not hit.
#include "maz/math/SweptSphere.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::SphereSweepHit;
using maz::math::sweepSpherePlane;
using maz::math::vec3;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static bool vnear(const vec3& a, const vec3& b, float e = 1e-4f) {
    return std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}

int main() {
    const vec3 up(0, 1, 0); // ground plane y = 0

    // --- 1. Falling toward the ground: exact impact time and point. ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, -10, 0), up, 0.0f);
        CHECK(h.hit, "falling sphere hits the ground");
        CHECK(near(h.t, 0.4f), "impact at t = (5-1)/10 = 0.4");
        CHECK(vnear(h.point, vec3(0, 0, 0)), "contact point on the plane");
    }

    // --- 2. Already overlapping -> t = 0. ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 0.5f, 0), 1.0f, vec3(0, -10, 0), up, 0.0f);
        CHECK(h.hit && near(h.t, 0.0f), "already-touching sphere reports t=0");
        CHECK(vnear(h.point, vec3(0, 0, 0)), "contact projects the center to the plane");
    }

    // --- 3. Moving away -> no hit. ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, 10, 0), up, 0.0f);
        CHECK(!h.hit, "sphere moving away does not hit");
    }

    // --- 4. Too slow to reach the plane this step. ---
    {
        // dist 5, radius 1: needs to travel 4 to touch, but only moves 1 this step (t would be 4).
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(0, -1, 0), up, 0.0f);
        CHECK(!h.hit, "sphere that can't reach the plane within the step does not hit");
    }

    // --- 5. Approaching from below (other side). ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, -5, 0), 1.0f, vec3(0, 10, 0), up, 0.0f);
        CHECK(h.hit && near(h.t, 0.4f), "from below: impact at t=0.4");
        CHECK(vnear(h.point, vec3(0, 0, 0)), "contact on the plane from below");
    }

    // --- 6. Parallel motion -> no hit. ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 5, 0), 1.0f, vec3(10, 0, 0), up, 0.0f);
        CHECK(!h.hit, "parallel motion never reaches the plane");
    }

    // --- 7. Non-zero plane offset (floor at y = 2). ---
    {
        const SphereSweepHit h = sweepSpherePlane(vec3(0, 8, 0), 1.0f, vec3(0, -10, 0), up, 2.0f);
        // center dist = 8-2 = 6; touch after travelling 5; t = 5/10 = 0.5; point at y=2.
        CHECK(h.hit && near(h.t, 0.5f), "offset plane: t = (6-1)/10 = 0.5");
        CHECK(vnear(h.point, vec3(0, 2, 0)), "contact on the offset plane");
    }

    if (g_fail == 0) {
        std::printf("sweptsphere: OK — fall impact, overlap t=0, moving away, too slow, from below, "
                    "parallel, offset plane.\n");
        return 0;
    }
    std::printf("sweptsphere: %d failure(s).\n", g_fail);
    return 1;
}
