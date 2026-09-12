// tests/math/triangleintersect.cpp — verifies triangle-triangle intersection (math::trianglesIntersect),
// Möller's test. Ground truths on hand-constructed triangles where the answer is obvious:
//   * two triangles in perpendicular planes that pierce each other -> intersect;
//   * the same pair pulled apart along the piercing axis -> separated;
//   * one triangle fully above another's plane -> separated (the fast plane reject);
//   * coplanar overlapping / coplanar disjoint -> intersect / separated (the 2D fallback);
//   * a shared edge and one triangle nested inside another (coplanar) -> intersect (touching counts);
//   * a vertex poking just through vs. just short of a face -> intersect / separated;
//   * symmetry: trianglesIntersect(A,B) == trianglesIntersect(B,A) for every case;
//   * a degenerate (zero-area) triangle never reports an intersection.
#include "maz/math/TriangleIntersect.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::trianglesIntersect;
using maz::math::vec3;

// Assert the expected result AND symmetry (swapping the two triangles must agree).
static void expect(bool want, const vec3& a0, const vec3& a1, const vec3& a2, const vec3& b0, const vec3& b1,
                   const vec3& b2, const char* msg) {
    const bool ab = trianglesIntersect(a0, a1, a2, b0, b1, b2);
    const bool ba = trianglesIntersect(b0, b1, b2, a0, a1, a2);
    CHECK(ab == want, msg);
    CHECK(ab == ba, "trianglesIntersect is symmetric in its arguments");
}

int main() {
    // Triangle T lies in the XY plane (z = 0), a right triangle spanning [0,2] x [0,2].
    const vec3 t0(0, 0, 0), t1(2, 0, 0), t2(0, 2, 0);

    // --- 1. A vertical triangle that stabs down through T's interior. ---
    {
        const vec3 s0(0.5f, 0.5f, 1.0f), s1(0.7f, 0.5f, 1.0f), s2(0.6f, 0.5f, -1.0f);
        expect(true, t0, t1, t2, s0, s1, s2, "a triangle piercing the interior intersects");
    }

    // --- 2. Same vertical triangle lifted entirely above z = 0 -> no intersection (plane reject). ---
    {
        const vec3 s0(0.5f, 0.5f, 1.0f), s1(0.7f, 0.5f, 1.0f), s2(0.6f, 0.5f, 0.25f);
        expect(false, t0, t1, t2, s0, s1, s2, "a triangle fully above the plane does not intersect");
    }

    // --- 3. Vertical triangle piercing the plane but OUTSIDE T's area (far in +x) -> no. ---
    {
        const vec3 s0(5.0f, 0.5f, 1.0f), s1(5.2f, 0.5f, 1.0f), s2(5.1f, 0.5f, -1.0f);
        expect(false, t0, t1, t2, s0, s1, s2, "piercing the plane outside the triangle does not intersect");
    }

    // --- 4. Coplanar, overlapping -> intersect (2D fallback). ---
    {
        const vec3 c0(0.5f, 0.5f, 0), c1(2.5f, 0.5f, 0), c2(0.5f, 2.5f, 0);
        expect(true, t0, t1, t2, c0, c1, c2, "coplanar overlapping triangles intersect");
    }

    // --- 5. Coplanar, disjoint -> no. ---
    {
        const vec3 c0(5, 5, 0), c1(6, 5, 0), c2(5, 6, 0);
        expect(false, t0, t1, t2, c0, c1, c2, "coplanar disjoint triangles do not intersect");
    }

    // --- 6. Coplanar, one nested wholly inside the other -> intersect (containment). ---
    {
        const vec3 c0(0.3f, 0.3f, 0), c1(0.6f, 0.3f, 0), c2(0.3f, 0.6f, 0);
        expect(true, t0, t1, t2, c0, c1, c2, "a coplanar triangle nested inside another intersects");
    }

    // --- 7. Sharing an edge (coplanar, mirrored across the x-axis edge) -> touching counts as intersect. ---
    {
        const vec3 c0(0, 0, 0), c1(2, 0, 0), c2(1, -2, 0);
        expect(true, t0, t1, t2, c0, c1, c2, "triangles sharing an edge count as intersecting");
    }

    // --- 8. A vertex poking just THROUGH the face vs. just SHORT of it. ---
    {
        // A small triangle whose lowest vertex dips to z = -0.01 at (0.5, 0.5) — just through.
        const vec3 p0(0.5f, 0.5f, -0.01f), p1(0.4f, 0.5f, 1.0f), p2(0.6f, 0.5f, 1.0f);
        expect(true, t0, t1, t2, p0, p1, p2, "a vertex poking just through the face intersects");
        // Raise the tip to z = +0.01 — now entirely above, just short.
        const vec3 q0(0.5f, 0.5f, 0.01f), q1(0.4f, 0.5f, 1.0f), q2(0.6f, 0.5f, 1.0f);
        expect(false, t0, t1, t2, q0, q1, q2, "a vertex stopping just short of the face does not intersect");
    }

    // --- 9. Two perpendicular triangles forming an interlocking X (classic case) -> intersect. ---
    {
        const vec3 a0(-1, 0, 0), a1(1, 0, 0), a2(0, 1, 0);   // in XY plane
        const vec3 b0(0, -1, 0.0f), b1(0, 1, 0.0f), b2(0, 0.5f, 2.0f); // in YZ-ish plane crossing it
        expect(true, a0, a1, a2, b0, b1, b2, "interlocking perpendicular triangles intersect");
    }

    // --- 10. A degenerate (zero-area) triangle never intersects. ---
    {
        const vec3 d0(0.5f, 0.5f, -1), d1(0.5f, 0.5f, 1), d2(0.5f, 0.5f, 0); // all colinear along z
        expect(false, t0, t1, t2, d0, d1, d2, "a degenerate triangle reports no intersection");
    }

    if (g_fail == 0) {
        std::printf("triangleintersect: OK — Möller tri-tri correct across piercing/coplanar/touching/degenerate.\n");
        return 0;
    }
    std::printf("triangleintersect: %d failure(s).\n", g_fail);
    return 1;
}
