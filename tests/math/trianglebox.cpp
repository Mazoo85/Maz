// tests/math/trianglebox.cpp — verifies triangle-AABB overlap (math::triangleIntersectsAabb), Akenine-Möller
// SAT. Ground truths against the box centred at origin with half-extents (1,1,1) — i.e. [-1,1]^3:
//   * a small triangle wholly inside the box overlaps;
//   * a triangle far outside does not;
//   * a big triangle whose edge passes through the box (no vertex inside) overlaps;
//   * THE discriminating case: a triangle whose AABB overlaps the box but whose body misses it (separated by a
//     diagonal edge-cross axis) does NOT overlap — a naive AABB-vs-AABB test would wrongly say yes;
//   * a triangle lying in the box's top face plane, overlapping in 2D, overlaps (touching counts);
//   * a triangle a hair above the box does not (the box face-normal axis separates);
//   * a vertex touching the box surface counts as overlap;
//   * the Aabb3 overload agrees with the center/half form.
#include "maz/math/TriangleBox.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::triangleIntersectsAabb;
using maz::math::vec3;
using maz::math::Aabb3;

int main() {
    const vec3 c(0, 0, 0), h(1, 1, 1); // box [-1,1]^3

    // 1. Wholly inside.
    CHECK(triangleIntersectsAabb(vec3(-0.5f, 0, 0), vec3(0.5f, 0, 0), vec3(0, 0.5f, 0), c, h),
          "a triangle inside the box overlaps");

    // 2. Far outside.
    CHECK(!triangleIntersectsAabb(vec3(5, 5, 5), vec3(6, 5, 5), vec3(5, 6, 5), c, h),
          "a triangle far outside does not overlap");

    // 3. Big triangle, all vertices outside, but an edge cuts through the box.
    CHECK(triangleIntersectsAabb(vec3(-3, 0, 0), vec3(3, 0, 0), vec3(0, 3, 0), c, h),
          "a triangle whose edge passes through the box overlaps");

    // 4. Discriminating case: AABB overlaps the box, but the triangle body misses it.
    // Triangle (3,0),(0,3),(3,3) in z=0: its AABB is [0,3]x[0,3]x{0}, overlapping the box's [0,1]x[0,1]x{0}
    // corner region — but the triangle lives on the x+y>=3 side while the box only reaches x+y=2, so they are
    // separated by the (1,1,0) edge-cross axis.
    CHECK(!triangleIntersectsAabb(vec3(3, 0, 0), vec3(0, 3, 0), vec3(3, 3, 0), c, h),
          "a triangle whose AABB overlaps but whose body misses does NOT overlap");

    // 5. Coplanar with the top face (z = 1), overlapping in 2D.
    CHECK(triangleIntersectsAabb(vec3(-0.5f, -0.5f, 1), vec3(0.5f, -0.5f, 1), vec3(0, 0.5f, 1), c, h),
          "a triangle in the box's top-face plane overlaps");

    // 6. A hair above the box.
    CHECK(!triangleIntersectsAabb(vec3(-0.5f, -0.5f, 1.01f), vec3(0.5f, -0.5f, 1.01f), vec3(0, 0.5f, 1.01f), c, h),
          "a triangle just above the box does not overlap");

    // 7. A single vertex touching the +x face.
    CHECK(triangleIntersectsAabb(vec3(1, 0, 0), vec3(3, 0, 0), vec3(3, 2, 0), c, h),
          "a vertex touching the box surface counts as overlap");

    // 8. A triangle slab cutting through the box interior.
    CHECK(triangleIntersectsAabb(vec3(-3, -3, 0), vec3(3, -3, 0), vec3(0, 3, 0), c, h),
          "a large triangle covering the box centre overlaps");

    // 9. The Aabb3 overload agrees with the center/half form on a spread of triangles.
    {
        const Aabb3 box(vec3(-1, -1, -1), vec3(1, 1, 1));
        const vec3 tris[4][3] = {
            {vec3(-0.5f, 0, 0), vec3(0.5f, 0, 0), vec3(0, 0.5f, 0)},   // inside
            {vec3(5, 5, 5), vec3(6, 5, 5), vec3(5, 6, 5)},            // far
            {vec3(3, 0, 0), vec3(0, 3, 0), vec3(3, 3, 0)},            // AABB-overlap-but-miss
            {vec3(-3, 0, 0), vec3(3, 0, 0), vec3(0, 3, 0)},           // edge through box
        };
        bool agree = true;
        for (auto& t : tris) {
            const bool a = triangleIntersectsAabb(t[0], t[1], t[2], box);
            const bool b = triangleIntersectsAabb(t[0], t[1], t[2], c, h);
            if (a != b) agree = false;
        }
        CHECK(agree, "the Aabb3 overload matches the center/half form");
    }

    if (g_fail == 0) {
        std::printf("trianglebox: OK — SAT triangle-AABB overlap correct, incl. the AABB-overlap-but-miss case.\n");
        return 0;
    }
    std::printf("trianglebox: %d failure(s).\n", g_fail);
    return 1;
}
