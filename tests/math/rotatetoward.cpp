// tests/math/rotatetoward.cpp — verifies capped-rate direction turning (math::rotateToward, vec2 & vec3).
// Ground truths: a turn smaller than the gap advances exactly maxRadians along the shortest arc and preserves
// the source vector's LENGTH; a turn at least as large as the gap snaps exactly onto the target's heading
// (with the source length); a negative rate turns away; zero-length inputs are returned unchanged; the 3D
// antiparallel case stays finite and well-defined. Checked against hand-computed rotations. Pure CPU.
#include "maz/math/VectorOps.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec2;
using maz::math::vec3;
namespace mm = maz::math;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static float len2(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static float len3(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    const float pi = 3.14159265358979323846f;

    // --- vec2: partial turn advances exactly maxRadians, length preserved. ---
    {
        const vec2 r = mm::rotateToward(vec2(1, 0), vec2(0, 1), pi / 4.0f); // gap pi/2, turn pi/4
        CHECK(near(r.x, std::cos(pi / 4.0f)) && near(r.y, std::sin(pi / 4.0f)), "vec2 partial turn = 45 deg");
        CHECK(near(len2(r), 1.0f), "vec2 partial preserves length");
    }

    // --- vec2: turn >= gap snaps to the target heading, keeping the source length. ---
    {
        const vec2 r = mm::rotateToward(vec2(2, 0), vec2(0, 5), pi); // snaps to +y, length 2
        CHECK(near(r.x, 0.0f) && near(r.y, 2.0f), "vec2 snap to target heading with source length");
    }

    // --- vec2: negative rate turns AWAY from the target. ---
    {
        const vec2 r = mm::rotateToward(vec2(1, 0), vec2(0, 1), -pi / 4.0f); // away = clockwise
        CHECK(near(r.x, std::cos(-pi / 4.0f)) && near(r.y, std::sin(-pi / 4.0f)), "vec2 negative rate turns away");
    }

    // --- vec2: zero-length source or target returns the source unchanged. ---
    {
        CHECK(mm::rotateToward(vec2(0, 0), vec2(1, 0), 1.0f) == vec2(0, 0), "vec2 zero source unchanged");
        const vec2 s(3, 4);
        CHECK(mm::rotateToward(s, vec2(0, 0), 1.0f) == s, "vec2 zero target unchanged");
    }

    // --- vec3: partial turn about the shortest arc, length preserved, moves closer to the target. ---
    {
        const vec3 r = mm::rotateToward(vec3(1, 0, 0), vec3(0, 1, 0), pi / 4.0f);
        CHECK(near(r.x, std::cos(pi / 4.0f)) && near(r.y, std::sin(pi / 4.0f)) && near(r.z, 0.0f),
              "vec3 partial turn = 45 deg in xy-plane");
        CHECK(near(len3(r), 1.0f), "vec3 partial preserves length");
        // The angle from the result to the target must be smaller than from the original to the target.
        CHECK(mm::angleTo(r, vec3(0, 1, 0)) < mm::angleTo(vec3(1, 0, 0), vec3(0, 1, 0)) - 1e-4f,
              "vec3 partial moves toward target");
    }

    // --- vec3: turn >= gap snaps to target heading with source length. ---
    {
        const vec3 r = mm::rotateToward(vec3(2, 0, 0), vec3(0, 0, 3), pi);
        CHECK(near(r.x, 0.0f) && near(r.y, 0.0f) && near(r.z, 2.0f), "vec3 snap to target heading, length 2");
    }

    // --- vec3: small turn preserves length exactly. ---
    {
        const vec3 r = mm::rotateToward(vec3(3, 0, 0), vec3(0, 0, 10), 0.1f);
        CHECK(near(len3(r), 3.0f), "vec3 small turn preserves length");
    }

    // --- vec3: antiparallel input stays finite, well-defined, and length-preserving. ---
    {
        const vec3 r = mm::rotateToward(vec3(1, 0, 0), vec3(-1, 0, 0), pi / 2.0f);
        CHECK(std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.z), "vec3 antiparallel finite");
        CHECK(near(len3(r), 1.0f), "vec3 antiparallel preserves length");
        CHECK(near(mm::angleTo(vec3(1, 0, 0), r), pi / 2.0f), "vec3 antiparallel turned exactly 90 deg");
    }

    // --- vec3: zero-length inputs unchanged. ---
    {
        const vec3 s(1, 2, 2);
        CHECK(mm::rotateToward(vec3(0, 0, 0), s, 1.0f) == vec3(0, 0, 0), "vec3 zero source unchanged");
        CHECK(mm::rotateToward(s, vec3(0, 0, 0), 1.0f) == s, "vec3 zero target unchanged");
    }

    if (g_fail == 0) {
        std::printf("rotate-toward: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
