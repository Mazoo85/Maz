// tests/math/rayobb.cpp — verifies ray/oriented-box intersection (math::Obb::intersectRay). Ground truths on a
// box rotated 45° about Y (half-extents (1,1,1) at a known centre), cross-checked against the analytic entry
// distance and against Aabb3::intersectRay for the identity (unrotated) case:
//   * a ray aimed at a rotated face hits at the expected distance;
//   * a ray that would hit the box's world-AABB but misses the rotated box itself returns no hit;
//   * a ray pointing away misses; a ray starting inside returns t = 0;
//   * tMax clips a far hit to a miss;
//   * with identity rotation, Obb::intersectRay agrees with Aabb3::intersectRay.
#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Aabb3;
using maz::math::Obb;
using maz::math::vec3;
using maz::math::mat3;

// A rotation of `deg` degrees about the Y axis, as a column-major mat3 (columns = local axes in world space).
static mat3 rotY(float deg) {
    const float r = deg * 3.14159265358979323846f / 180.0f;
    const float c = std::cos(r), s = std::sin(r);
    mat3 m(1.0f);
    // local x = (c,0,-s), local y = (0,1,0), local z = (s,0,c); columns in glm are m[col].
    m[0][0] = c;  m[0][1] = 0; m[0][2] = -s;
    m[1][0] = 0;  m[1][1] = 1; m[1][2] = 0;
    m[2][0] = s;  m[2][1] = 0; m[2][2] = c;
    return m;
}

int main() {
    // Box centred at (5,0,0), half (1,1,1), rotated 45° about Y.
    Obb box;
    box.center = vec3(5, 0, 0);
    box.half = vec3(1, 1, 1);
    box.axes = rotY(45.0f);

    // A 45°-rotated unit box has its nearest corner (toward -x) at distance sqrt(2) from the centre along x.
    // So a ray from the origin along +x enters at x = 5 - sqrt(2).
    const float expected = 5.0f - std::sqrt(2.0f);

    // 1. Ray straight at the rotated box hits at the expected distance.
    {
        auto t = box.intersectRay(vec3(0, 0, 0), vec3(1, 0, 0));
        CHECK(t.has_value(), "ray hits the rotated box");
        if (t) CHECK(std::fabs(*t - expected) < 1e-3f, "entry distance matches the rotated-corner geometry");
    }

    // 2. Discriminating case: a ray that pierces the box's world-AABB but misses the rotated box. The 45° box
    // in x-z is a diamond inscribed in its world-AABB [3.586,6.414] x [-1,1] x [-1.414,1.414], with the 4 AABB
    // corners lying OUTSIDE the diamond. A diagonal ray from (8,0,0.9) along (-1,0,0.3) enters the AABB's +x
    // face and exits its +z face while staying in the +x,+z corner region the whole time — so it crosses the
    // AABB but never the rotated box. (Hand-checked: the box-local w coordinate stays ~1.9 > 1 throughout.)
    {
        auto tAabb = box.aabb().intersectRay(vec3(8, 0, 0.9f), vec3(-1, 0, 0.3f));
        auto tObb = box.intersectRay(vec3(8, 0, 0.9f), vec3(-1, 0, 0.3f));
        CHECK(tAabb.has_value(), "the ray does cross the box's world-AABB (setup sanity)");
        CHECK(!tObb.has_value(), "a ray crossing the world-AABB but missing the rotated box returns no hit");
    }

    // 3. Ray pointing away misses.
    CHECK(!box.intersectRay(vec3(0, 0, 0), vec3(-1, 0, 0)).has_value(), "a ray pointing away misses");

    // 4. Ray starting inside the box returns t = 0.
    {
        auto t = box.intersectRay(box.center, vec3(1, 0, 0));
        CHECK(t.has_value() && *t == 0.0f, "a ray starting inside enters at t = 0");
    }

    // 5. tMax clips a far hit to a miss.
    {
        CHECK(box.intersectRay(vec3(0, 0, 0), vec3(1, 0, 0), 100.0f).has_value(), "hit within a large tMax");
        CHECK(!box.intersectRay(vec3(0, 0, 0), vec3(1, 0, 0), expected - 0.1f).has_value(),
              "a hit beyond tMax is clipped to a miss");
    }

    // 6. Identity rotation: Obb::intersectRay agrees with Aabb3::intersectRay.
    {
        Obb id;
        id.center = vec3(0, 0, 0);
        id.half = vec3(1, 2, 3);
        id.axes = mat3(1.0f);
        const Aabb3 box2(vec3(-1, -2, -3), vec3(1, 2, 3));
        const vec3 origins[3] = {vec3(-5, 0, 0), vec3(0, 10, 0), vec3(4, 4, 4)};
        const vec3 dirs[3] = {vec3(1, 0, 0), vec3(0, -1, 0), vec3(-1, -1, -1)};
        bool agree = true;
        for (int i = 0; i < 3; ++i) {
            auto a = id.intersectRay(origins[i], dirs[i]);
            auto b = box2.intersectRay(origins[i], dirs[i]);
            if (a.has_value() != b.has_value()) { agree = false; break; }
            if (a && std::fabs(*a - *b) > 1e-4f) { agree = false; break; }
        }
        CHECK(agree, "identity-rotation OBB agrees with the AABB slab test");
    }

    if (g_fail == 0) {
        std::printf("rayobb: OK — ray/OBB entry distance correct, incl. the AABB-hit-but-OBB-miss case.\n");
        return 0;
    }
    std::printf("rayobb: %d failure(s).\n", g_fail);
    return 1;
}
