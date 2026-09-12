// tests/math/capsuleaabb.cpp — verifies capsule-vs-AABB overlap and segment-AABB distance
// (math::capsuleIntersectsAabb / squaredDistanceSegmentAabb). Ground truths on a unit box [0,1]^3: a segment
// piercing the box has distance 0; a capsule hovering above a face overlaps iff its radius reaches the face
// (discriminating on the exact gap); a segment running parallel above a face gives the perpendicular gap; a
// segment beside an edge gives the edge gap; a segment near a corner gives the corner (diagonal) gap; a far
// capsule misses; a zero-length capsule degenerates to sphere-vs-AABB (cross-checked against
// Aabb3::intersectsSphere); distance is symmetric in the endpoints; negative radius clamps. Pure CPU.
#include "maz/math/CapsuleAabb.hpp"

#include <cmath>
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec3;
using maz::math::Aabb3;
using maz::math::capsuleIntersectsAabb;
using maz::math::squaredDistanceSegmentAabb;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    const Aabb3 box(vec3(0, 0, 0), vec3(1, 1, 1));

    // --- 1. A segment piercing the box -> distance 0, overlaps at any radius. ---
    {
        const vec3 p0(0.5f, 0.5f, 2.0f), p1(0.5f, 0.5f, -2.0f);
        CHECK(near(squaredDistanceSegmentAabb(p0, p1, box), 0.0f), "piercing distance 0");
        CHECK(capsuleIntersectsAabb(p0, p1, 0.01f, box), "piercing overlaps at tiny radius");
    }

    // --- 2. Vertical segment hovering above the top face: gap = lowest point height above z=1. ---
    {
        const vec3 p0(0.5f, 0.5f, 4.0f), p1(0.5f, 0.5f, 2.0f); // nearest end at z=2, face at z=1 -> gap 1
        CHECK(near(std::sqrt(squaredDistanceSegmentAabb(p0, p1, box)), 1.0f), "hover gap = 1.0");
        CHECK(!capsuleIntersectsAabb(p0, p1, 0.99f, box), "r<gap: no overlap");
        CHECK(capsuleIntersectsAabb(p0, p1, 1.01f, box), "r>gap: overlap");
    }

    // --- 3. Segment parallel above the top face over the interior: perpendicular gap. ---
    {
        const vec3 p0(0.2f, 0.5f, 3.0f), p1(0.8f, 0.5f, 3.0f); // z=3 over the interior -> gap 2
        CHECK(near(std::sqrt(squaredDistanceSegmentAabb(p0, p1, box)), 2.0f), "parallel-over-face gap = 2");
    }

    // --- 4. Vertical segment beside a face (outside x): gap = horizontal distance to the box side. ---
    {
        const vec3 p0(-1.5f, 0.5f, 3.0f), p1(-1.5f, 0.5f, -3.0f); // x=-1.5, box side at x=0 -> gap 1.5
        CHECK(near(std::sqrt(squaredDistanceSegmentAabb(p0, p1, box)), 1.5f), "beside-face gap = 1.5");
    }

    // --- 5. Segment near a corner: diagonal gap to the nearest corner. ---
    {
        // A short segment sitting out past the (1,1,1) corner, offset (1,1,1) beyond it -> distance sqrt(3).
        const vec3 p0(2.0f, 2.0f, 2.0f), p1(3.0f, 3.0f, 3.0f); // nearest end (2,2,2), corner (1,1,1)
        CHECK(near(std::sqrt(squaredDistanceSegmentAabb(p0, p1, box)), std::sqrt(3.0f)), "corner gap sqrt(3)");
    }

    // --- 6. Far capsule misses. ---
    {
        const vec3 p0(50.0f, 50.0f, 50.0f), p1(50.0f, 50.0f, 40.0f);
        CHECK(!capsuleIntersectsAabb(p0, p1, 5.0f, box), "far capsule misses");
    }

    // --- 7. Zero-length capsule = sphere; cross-check against Aabb3::intersectsSphere. ---
    {
        const vec3 c(0.5f, 0.5f, 1.5f); // 0.5 above the top face
        CHECK(near(std::sqrt(squaredDistanceSegmentAabb(c, c, box)), 0.5f), "point-capsule gap 0.5");
        for (float r : {0.4f, 0.6f}) {
            CHECK(capsuleIntersectsAabb(c, c, r, box) == box.intersectsSphere(c, r),
                  "zero-length capsule matches sphere-vs-AABB");
        }
    }

    // --- 8. Symmetry and negative-radius clamp. ---
    {
        const vec3 p0(-1.0f, 0.3f, 1.7f), p1(1.4f, 2.2f, -0.5f);
        CHECK(near(squaredDistanceSegmentAabb(p0, p1, box), squaredDistanceSegmentAabb(p1, p0, box)),
              "distance symmetric in endpoints");
        const vec3 q0(0.5f, 0.5f, 1.001f), q1(0.5f, 0.5f, 2.0f);
        CHECK(!capsuleIntersectsAabb(q0, q1, -3.0f, box), "negative radius clamps -> tiny gap misses");
    }

    if (g_fail == 0) {
        std::printf("capsule-aabb: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
