// tests/math/raysphere.cpp — verifies ray/sphere intersection (math::intersectRaySphere). Ground truths on a
// unit sphere at the origin (radius 1) and a shifted sphere, cross-checked against analytic entry distances:
//   * a ray aimed at the sphere hits at the near-surface distance;
//   * a ray pointing away misses; a ray offset by more than the radius misses;
//   * a grazing ray tangent at exactly the radius hits once;
//   * a ray starting inside returns the exit-point distance (its first forward crossing);
//   * tMax clips a far hit to a miss;
//   * a non-unit direction reports t in units of |dir|;
//   * a zero-length direction is a safe miss.
#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::intersectRaySphere;
using maz::math::vec3;

int main() {
    const vec3 c(0, 0, 0);
    const float r = 1.0f;

    // 1. Head-on hit: from (0,0,-5) along +z, near surface at z=-1 -> t = 4.
    {
        auto t = intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, 1), c, r);
        CHECK(t.has_value(), "head-on ray hits the sphere");
        if (t) CHECK(std::fabs(*t - 4.0f) < 1e-4f, "entry distance is 4 (surface at z=-1)");
    }

    // 2. Pointing away misses.
    CHECK(!intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, -1), c, r).has_value(), "ray pointing away misses");

    // 3. Offset beyond the radius misses (parallel to +z at y = 1.5 > r).
    CHECK(!intersectRaySphere(vec3(0, 1.5f, -5), vec3(0, 0, 1), c, r).has_value(), "ray offset beyond radius misses");

    // 4. Grazing tangent at exactly the radius (y = 1): a single touch at z = 0 -> t = 5.
    {
        auto t = intersectRaySphere(vec3(0, 1, -5), vec3(0, 0, 1), c, r);
        CHECK(t.has_value(), "a tangent ray grazes the sphere");
        if (t) CHECK(std::fabs(*t - 5.0f) < 1e-3f, "tangent touch is at t = 5 (z = 0)");
    }

    // 5. Ray starting inside returns the exit distance: from centre along +z -> exit at z=1 -> t = 1.
    {
        auto t = intersectRaySphere(c, vec3(0, 0, 1), c, r);
        CHECK(t.has_value() && std::fabs(*t - 1.0f) < 1e-4f, "a ray from inside returns the exit distance (t=1)");
    }

    // 6. tMax clips a far hit to a miss.
    {
        CHECK(intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, 1), c, r, 100.0f).has_value(), "hit within large tMax");
        CHECK(!intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, 1), c, r, 3.0f).has_value(),
              "a hit beyond tMax (4 > 3) is clipped to a miss");
    }

    // 7. Non-unit direction: t is in units of |dir|. dir=(0,0,2) from z=-5, surface z=-1 -> -5+2t=-1 -> t=2.
    {
        auto t = intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, 2), c, r);
        CHECK(t.has_value() && std::fabs(*t - 2.0f) < 1e-4f, "non-unit direction reports t in units of |dir|");
    }

    // 8. Zero-length direction is a safe miss (no divide-by-zero).
    CHECK(!intersectRaySphere(vec3(0, 0, -5), vec3(0, 0, 0), c, r).has_value(), "zero-length direction is a safe miss");

    // 9. A shifted sphere behaves the same (centre (10,0,0), r=2): from (10,0,-9) along +z? Use x-axis:
    // from (0,0,0) along +x at a sphere centred (10,0,0) r=2 -> near surface x=8 -> t=8.
    {
        auto t = intersectRaySphere(vec3(0, 0, 0), vec3(1, 0, 0), vec3(10, 0, 0), 2.0f);
        CHECK(t.has_value() && std::fabs(*t - 8.0f) < 1e-4f, "shifted sphere: entry distance is 8");
    }

    if (g_fail == 0) {
        std::printf("raysphere: OK — ray/sphere entry distance correct incl. tangent, inside, tMax, non-unit dir.\n");
        return 0;
    }
    std::printf("raysphere: %d failure(s).\n", g_fail);
    return 1;
}
