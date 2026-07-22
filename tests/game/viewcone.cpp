// tests/game/viewcone.cpp — verifies the vision-cone perception test (game::inViewCone2D/3D + sampleViewCone2D).
// Ground truths: a target dead-ahead within range is seen; behind is not; the cone edge (angle == halfAngle) is
// inclusive and just past it is excluded; out-of-range is not seen even when dead-ahead; the sample reports the
// correct distance and angle; a zero-length facing sees nothing; a coincident target is seen; 3D behaves the same.
// Angles/distances checked against the geometry. Pure CPU, deterministic.
#include "maz/game/ViewCone.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz;

int main() {
    const math::vec2 eye(0, 0);
    const math::vec2 facing(1, 0);          // looking along +X
    const float halfAngle = 3.14159265f / 4.0f; // 45 degrees
    const float range = 10.0f;

    // --- 1. Dead ahead within range -> seen; behind -> not. ---
    {
        CHECK(game::inViewCone2D(eye, facing, math::vec2(5, 0), halfAngle, range), "target dead-ahead is seen");
        CHECK(!game::inViewCone2D(eye, facing, math::vec2(-5, 0), halfAngle, range), "target behind is not seen");
    }

    // --- 2. Cone edge is inclusive; just past it is excluded. ---
    {
        // At exactly 45 degrees off-axis (dx=dy): inside (<=). At 60 degrees: outside.
        CHECK(game::inViewCone2D(eye, facing, math::vec2(5, 5), halfAngle, range), "the 45-degree edge is inside (<=)");
        const float a = 3.14159265f / 3.0f; // 60 degrees
        CHECK(!game::inViewCone2D(eye, facing, math::vec2(std::cos(a) * 5, std::sin(a) * 5), halfAngle, range),
              "60 degrees off-axis is outside a 45-degree cone");
    }

    // --- 3. Out of range is not seen even when dead-ahead. ---
    {
        CHECK(!game::inViewCone2D(eye, facing, math::vec2(20, 0), halfAngle, range), "beyond range is not seen");
        CHECK(game::inViewCone2D(eye, facing, math::vec2(10, 0), halfAngle, range), "exactly at range is seen (<=)");
    }

    // --- 4. sampleViewCone2D reports the right distance & angle. ---
    {
        const game::ViewSample s = game::sampleViewCone2D(eye, facing, math::vec2(3, 3), halfAngle, range);
        CHECK(std::fabs(s.distance - std::sqrt(18.0f)) < 1e-3f, "sample distance is correct");
        CHECK(std::fabs(s.angle - 3.14159265f / 4.0f) < 1e-3f, "sample angle is 45 degrees for (3,3)");
        CHECK(s.inCone, "sample marks it in-cone");
    }

    // --- 5. Degenerate cases. ---
    {
        CHECK(!game::inViewCone2D(eye, math::vec2(0, 0), math::vec2(5, 0), halfAngle, range), "zero facing sees nothing");
        CHECK(game::inViewCone2D(eye, facing, eye, halfAngle, range), "a coincident target is seen");
    }

    // --- 6. 3D behaves the same. ---
    {
        const math::vec3 o(0, 0, 0), f(0, 0, 1); // looking along +Z
        CHECK(game::inViewCone3D(o, f, math::vec3(0, 0, 5), halfAngle, range), "3D: dead-ahead is seen");
        CHECK(!game::inViewCone3D(o, f, math::vec3(0, 0, -5), halfAngle, range), "3D: behind is not seen");
        // 45 degrees in the Y-Z plane is on the edge.
        CHECK(game::inViewCone3D(o, f, math::vec3(0, 5, 5), halfAngle, range), "3D: 45-degree edge is inside");
        CHECK(!game::inViewCone3D(o, f, math::vec3(0, 0, 20), halfAngle, range), "3D: beyond range is not seen");
    }

    if (g_fail == 0) {
        std::printf("viewcone: OK — ahead/behind, edge-inclusive, range, sample distance/angle, degenerate, 3D.\n");
        return 0;
    }
    std::printf("viewcone: %d failure(s).\n", g_fail);
    return 1;
}
