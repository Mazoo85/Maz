// tests/math/sweptaabb.cpp — verifies swept AABB-vs-AABB continuous collision (math::sweptAabbAabb).
// Ground truths (hand-computed): a head-on approach hits at t = gap/speed with the face normal opposing
// travel; a contact landing exactly at t=1 is a hit while just-short misses; moving away never hits; a
// glancing pass that is separated on a non-motion axis misses; a diagonal approach hits at the shared entry
// time; an approach from the +x side moving -x yields a +x normal; boxes already overlapping at t=0 report no
// hit (documented semantics); zero motion with separation misses. Pure CPU, deterministic.
#include "maz/math/SweptAabb.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec3;
using maz::math::Aabb3;
using maz::math::sweptAabbAabb;
using maz::math::SweptAabbHit;

static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

int main() {
    const Aabb3 unit(vec3(0, 0, 0), vec3(1, 1, 1)); // moving unit cube at the origin

    // --- 1. Head-on +x: gap 4 (x=1 -> x=5), speed 10 -> t=0.4, normal -x. ---
    {
        const Aabb3 s(vec3(5, 0, 0), vec3(6, 1, 1));
        const SweptAabbHit h = sweptAabbAabb(unit, vec3(10, 0, 0), s);
        CHECK(h.hit, "head-on hits");
        CHECK(near(h.t, 0.4f), "head-on t=0.4");
        CHECK(near(h.normal.x, -1.0f) && near(h.normal.y, 0.0f) && near(h.normal.z, 0.0f),
              "head-on normal -x");
    }

    // --- 2. Contact exactly at t=1 hits; just-short misses. ---
    {
        const Aabb3 s(vec3(5, 0, 0), vec3(6, 1, 1)); // gap = 4
        const SweptAabbHit hit1 = sweptAabbAabb(unit, vec3(4, 0, 0), s); // 4/4 = 1.0
        CHECK(hit1.hit && near(hit1.t, 1.0f), "reaches exactly at t=1");
        const SweptAabbHit miss = sweptAabbAabb(unit, vec3(3.9f, 0, 0), s); // 4/3.9 > 1
        CHECK(!miss.hit, "just short of reaching -> miss");
    }

    // --- 3. Moving away never hits. ---
    {
        const Aabb3 s(vec3(5, 0, 0), vec3(6, 1, 1));
        CHECK(!sweptAabbAabb(unit, vec3(-10, 0, 0), s).hit, "moving away misses");
    }

    // --- 4. Glancing pass: separated on the y axis (no y motion) -> miss. ---
    {
        const Aabb3 s(vec3(5, 5, 5), vec3(6, 6, 6));
        CHECK(!sweptAabbAabb(unit, vec3(10, 0, 0), s).hit, "glancing pass misses");
    }

    // --- 5. Diagonal approach: both x and y enter at 0.4 -> corner contact at t=0.4. ---
    {
        const Aabb3 s(vec3(5, 5, 0), vec3(6, 6, 1));
        const SweptAabbHit h = sweptAabbAabb(unit, vec3(10, 10, 0), s);
        CHECK(h.hit && near(h.t, 0.4f), "diagonal corner contact t=0.4");
    }

    // --- 6. Approach from the +x side moving -x: +x normal, t = 9/20 = 0.45. ---
    {
        const Aabb3 mover(vec3(10, 0, 0), vec3(11, 1, 1));
        const Aabb3 s(vec3(0, 0, 0), vec3(1, 1, 1)); // static to the -x of the mover
        const SweptAabbHit h = sweptAabbAabb(mover, vec3(-20, 0, 0), s);
        CHECK(h.hit && near(h.t, 0.45f), "-x approach t=0.45");
        CHECK(near(h.normal.x, 1.0f), "-x approach normal +x");
    }

    // --- 7. Boxes already overlapping at t=0 -> no first-contact in the step. ---
    {
        const Aabb3 s(vec3(0.5f, 0.5f, 0.5f), vec3(1.5f, 1.5f, 1.5f)); // overlaps `unit`
        CHECK(!sweptAabbAabb(unit, vec3(1, 0, 0), s).hit, "already overlapping -> no hit");
    }

    // --- 8. Zero motion + separation -> miss; and a purely tangential slide is not a hit. ---
    {
        const Aabb3 s(vec3(5, 0, 0), vec3(6, 1, 1));
        CHECK(!sweptAabbAabb(unit, vec3(0, 0, 0), s).hit, "no motion, separated -> miss");
        // Slide parallel along y while x stays touching-but-separated: x gap never closes -> miss.
        const Aabb3 s2(vec3(2, 0, 0), vec3(3, 1, 1));
        CHECK(!sweptAabbAabb(unit, vec3(0, 10, 0), s2).hit, "pure tangential slide -> miss");
    }

    if (g_fail == 0) {
        std::printf("swept-aabb: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
