// tests/math/capsuletriangle.cpp — verifies capsule-vs-triangle overlap and the segment-triangle distance
// (math::capsuleIntersectsTriangle / squaredDistanceSegmentTriangle). Ground truths on a unit triangle in the
// z=0 plane: a segment piercing the interior has distance 0; a capsule hovering above the interior overlaps iff
// its radius reaches the plane (discriminating on the exact gap); a segment parallel above the interior gives
// the perpendicular gap; a segment passing beside an edge gives the edge distance; a capsule far to the side
// misses; the radius clamp and degenerate (point) capsule behave; results are symmetric in the segment's
// endpoints. Distances cross-checked against hand-computed values. Pure CPU, deterministic.
#include "maz/math/CapsuleTriangle.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec3;
using maz::math::capsuleIntersectsTriangle;
using maz::math::squaredDistanceSegmentTriangle;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    // Triangle in the z=0 plane, comfortably enclosing the origin's projection.
    const vec3 a(0.0f, 0.0f, 0.0f), b(4.0f, 0.0f, 0.0f), c(0.0f, 4.0f, 0.0f);
    const vec3 inside(1.0f, 1.0f, 0.0f); // (1,1) is inside the triangle x>=0,y>=0,x+y<=4

    // --- 1. A segment piercing the triangle interior -> distance 0, overlaps for any radius. ---
    {
        const vec3 p0 = inside + vec3(0, 0, 2.0f);
        const vec3 p1 = inside + vec3(0, 0, -2.0f);
        CHECK(near(squaredDistanceSegmentTriangle(p0, p1, a, b, c), 0.0f), "piercing segment distance 0");
        CHECK(capsuleIntersectsTriangle(p0, p1, 0.01f, a, b, c), "piercing overlaps even at tiny radius");
    }

    // --- 2. A vertical segment hovering above the interior: gap = its lowest point's height. ---
    {
        const vec3 p0 = inside + vec3(0, 0, 3.0f);
        const vec3 p1 = inside + vec3(0, 0, 1.0f); // closest point is 1.0 above the plane
        CHECK(near(std::sqrt(squaredDistanceSegmentTriangle(p0, p1, a, b, c)), 1.0f), "hover gap = 1.0");
        // Discriminating radius test: r just under vs just over the gap.
        CHECK(!capsuleIntersectsTriangle(p0, p1, 0.99f, a, b, c), "r<gap: no overlap");
        CHECK(capsuleIntersectsTriangle(p0, p1, 1.01f, a, b, c), "r>gap: overlap");
    }

    // --- 3. A segment parallel to the plane, running above the interior: perpendicular gap. ---
    {
        const vec3 p0(0.5f, 1.0f, 2.0f);
        const vec3 p1(2.0f, 1.0f, 2.0f); // both x in-range, y=1 inside; height 2 everywhere
        CHECK(near(std::sqrt(squaredDistanceSegmentTriangle(p0, p1, a, b, c)), 2.0f),
              "parallel-over-interior gap = 2.0");
    }

    // --- 4. A vertical segment beside the triangle (outside an edge): distance = horizontal gap. ---
    {
        // x=-1.5 is outside the x=0 edge (the a-c edge). Nearest point on triangle is on that edge at (0,1,0).
        const vec3 p0(-1.5f, 1.0f, 3.0f);
        const vec3 p1(-1.5f, 1.0f, -3.0f); // crosses the plane but outside the triangle -> not a pierce
        CHECK(near(std::sqrt(squaredDistanceSegmentTriangle(p0, p1, a, b, c)), 1.5f), "beside-edge gap = 1.5");
        CHECK(!capsuleIntersectsTriangle(p0, p1, 1.0f, a, b, c), "beside edge, r=1 misses");
        CHECK(capsuleIntersectsTriangle(p0, p1, 2.0f, a, b, c), "beside edge, r=2 reaches");
    }

    // --- 5. A capsule far away misses regardless of a modest radius. ---
    {
        const vec3 p0(100.0f, 100.0f, 100.0f), p1(100.0f, 100.0f, 90.0f);
        CHECK(!capsuleIntersectsTriangle(p0, p1, 5.0f, a, b, c), "far capsule misses");
    }

    // --- 6. Degenerate (zero-length) capsule = sphere vs triangle. ---
    {
        const vec3 pt = inside + vec3(0, 0, 0.5f); // 0.5 above interior
        CHECK(near(std::sqrt(squaredDistanceSegmentTriangle(pt, pt, a, b, c)), 0.5f), "point-capsule gap 0.5");
        CHECK(capsuleIntersectsTriangle(pt, pt, 0.6f, a, b, c), "sphere r=0.6 reaches");
        CHECK(!capsuleIntersectsTriangle(pt, pt, 0.4f, a, b, c), "sphere r=0.4 misses");
    }

    // --- 7. Symmetry: swapping the segment endpoints doesn't change the distance. ---
    {
        const vec3 p0(0.5f, 0.5f, 1.3f), p1(2.5f, 0.2f, -0.7f);
        CHECK(near(squaredDistanceSegmentTriangle(p0, p1, a, b, c),
                   squaredDistanceSegmentTriangle(p1, p0, a, b, c)),
              "distance symmetric in endpoints");
    }

    // --- 8. Negative radius is clamped to 0 (touch-only). ---
    {
        const vec3 p0 = inside + vec3(0, 0, 0.001f), p1 = inside + vec3(0, 0, 2.0f);
        CHECK(!capsuleIntersectsTriangle(p0, p1, -5.0f, a, b, c), "negative radius clamped, tiny gap misses");
    }

    if (g_fail == 0) {
        std::printf("capsule-triangle: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
