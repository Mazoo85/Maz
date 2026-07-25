// tests/math/sphereaabb.cpp — verifies sphere-vs-AABB overlap and point-box squared distance
// (math::Aabb3::intersectsSphere / distanceSquared). Ground truths on the box [-1,1]^3:
//   * distanceSquared is 0 inside, and the exact clamped distance for face / edge / corner points;
//   * a sphere centred inside overlaps; one whose centre is just outside a face by < radius overlaps;
//   * a sphere clearly separated does not;
//   * THE corner case: a sphere off a box corner whose centre is within radius on each axis SEPARATELY but
//     farther than radius from the actual corner does NOT overlap (a per-axis expand test gets this wrong);
//   * touching (distance exactly radius) counts as overlap.
#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Aabb3;
using maz::math::vec3;

int main() {
    const Aabb3 box(vec3(-1, -1, -1), vec3(1, 1, 1));

    // 1. distanceSquared: inside is 0; face/edge/corner distances are exact.
    CHECK(box.distanceSquared(vec3(0, 0, 0)) == 0.0f, "distanceSquared is 0 for an interior point");
    CHECK(std::fabs(box.distanceSquared(vec3(3, 0, 0)) - 4.0f) < 1e-5f, "face distance^2 = 2^2 = 4");
    CHECK(std::fabs(box.distanceSquared(vec3(3, 3, 0)) - 8.0f) < 1e-5f, "edge distance^2 = 2^2 + 2^2 = 8");
    CHECK(std::fabs(box.distanceSquared(vec3(3, 3, 3)) - 12.0f) < 1e-5f, "corner distance^2 = 3*2^2 = 12");

    // 2. Basic overlap: centre inside, centre just outside a face by < radius.
    CHECK(box.intersectsSphere(vec3(0, 0, 0), 0.5f), "a sphere centred inside overlaps");
    CHECK(box.intersectsSphere(vec3(1.4f, 0, 0), 0.5f), "a sphere just outside a face (0.4 < 0.5) overlaps");

    // 3. Clearly separated.
    CHECK(!box.intersectsSphere(vec3(5, 0, 0), 1.0f), "a distant sphere does not overlap");
    CHECK(!box.intersectsSphere(vec3(1.6f, 0, 0), 0.5f), "a sphere just past a face (0.6 > 0.5) does not overlap");

    // 4. Corner discriminator: centre (2,2,2) is 1 unit past the box on each axis (< radius 1.5 per axis), but
    // the true distance to corner (1,1,1) is sqrt(3) ~ 1.732 > 1.5 — so NO overlap. A per-axis expand-by-radius
    // test would wrongly report a hit.
    CHECK(!box.intersectsSphere(vec3(2, 2, 2), 1.5f), "a sphere off a corner (within radius per-axis) does NOT overlap");
    // Grow the radius past the corner distance -> now it overlaps.
    CHECK(box.intersectsSphere(vec3(2, 2, 2), 1.8f), "growing the radius past the corner distance overlaps");

    // 5. Touching exactly (distance == radius) counts as overlap.
    CHECK(box.intersectsSphere(vec3(3, 0, 0), 2.0f), "a sphere touching a face exactly (dist == radius) overlaps");

    if (g_fail == 0) {
        std::printf("sphereaabb: OK — sphere/AABB overlap correct incl. the off-corner discriminator.\n");
        return 0;
    }
    std::printf("sphereaabb: %d failure(s).\n", g_fail);
    return 1;
}
