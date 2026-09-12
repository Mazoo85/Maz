// tests/game/flycamera.cpp — verifies the first-person fly camera (game::FlyCamera): where it looks,
// how it moves along its own axes, that pitch cannot flip over the top, and that the view matrix it
// produces really is the inverse of where the camera is standing. Ground truths are hand-computed
// against the engine's conventions — +Y is world up, yaw is measured in the XZ plane from +X, and
// the default camera faces -Z — so a change to any of those breaks a named check rather than
// silently re-aiming every 3D app. Deterministic CPU: no GPU, no window.
#include "maz/game/FlyCamera.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::FlyCamera;
using maz::math::vec3;
using maz::math::vec4;

static bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
static bool approxV(const vec3& a, const vec3& b, float eps = 1e-4f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

int main() {
    // --- 1. A fresh camera stands back from the origin, facing -Z and slightly down. ---
    {
        FlyCamera cam;
        CHECK(approxV(cam.position(), vec3(0.0f, 2.0f, 8.0f)), "default position");
        const vec3 f = cam.forward();
        CHECK(f.z < -0.9f, "default camera faces -Z");
        CHECK(f.y < 0.0f, "default camera looks slightly down");
        CHECK(approx(glm::length(f), 1.0f), "forward is a unit vector");
    }

    // --- 2. Yaw 0, pitch 0 looks straight down +X: the zero of the angle convention. ---
    {
        FlyCamera cam;
        cam.setYawPitch(0.0f, 0.0f);
        CHECK(approxV(cam.forward(), vec3(1.0f, 0.0f, 0.0f)), "yaw 0, pitch 0 faces +X");
    }

    // --- 3. Yaw sweeps the XZ plane; pitch lifts out of it. ---
    {
        FlyCamera cam;
        const float halfPi = 1.5707963f;
        cam.setYawPitch(halfPi, 0.0f);
        CHECK(approxV(cam.forward(), vec3(0.0f, 0.0f, 1.0f), 1e-3f), "yaw +90deg faces +Z");
        cam.setYawPitch(0.0f, halfPi * 0.5f);
        const vec3 f = cam.forward();
        CHECK(approx(f.y, std::sin(halfPi * 0.5f)), "pitch 45deg lifts forward.y to sin(45)");
        CHECK(approx(glm::length(f), 1.0f), "forward stays unit under pitch");
    }

    // --- 4. look() accumulates deltas rather than setting absolute angles. ---
    {
        FlyCamera cam;
        cam.setYawPitch(0.0f, 0.0f);
        cam.look(0.5f, 0.25f);
        cam.look(0.5f, 0.25f);
        const vec3 f = cam.forward();
        FlyCamera reference;
        reference.setYawPitch(1.0f, 0.5f);
        CHECK(approxV(f, reference.forward()), "two half-turns equal one whole one");
    }

    // --- 5. Pitch is clamped so the camera can never flip over the top. ---
    //     Without the clamp, looking far enough up rolls the view upside down, which is the one
    //     thing a first-person camera must never do to the person holding it.
    {
        FlyCamera cam;
        cam.setYawPitch(0.0f, 0.0f);
        cam.look(0.0f, 100.0f);
        CHECK(cam.forward().y > 0.99f, "looking up hard points almost straight up");
        CHECK(cam.forward().y < 1.0f, "but never all the way, which would be degenerate");
        cam.look(0.0f, -200.0f);
        CHECK(cam.forward().y < -0.99f, "and the same looking down");
        CHECK(cam.forward().y > -1.0f, "still short of straight down");
        CHECK(approx(glm::length(cam.forward()), 1.0f), "forward is unit at the clamp");
    }

    // --- 6. moveDelta reports the step WITHOUT taking it — the hook for collision. ---
    {
        FlyCamera cam;
        cam.setYawPitch(0.0f, 0.0f);
        cam.setPosition(vec3(0.0f, 0.0f, 0.0f));
        const vec3 d = cam.moveDelta(1.0f, 0.0f, 0.0f, 0.5f, 4.0f);
        CHECK(approxV(cam.position(), vec3(0.0f, 0.0f, 0.0f)), "moveDelta must not move the camera");
        CHECK(approxV(d, vec3(2.0f, 0.0f, 0.0f)), "forward 1 at speed 4 for 0.5s is 2 units along +X");
    }

    // --- 7. move() applies exactly what moveDelta reported. ---
    {
        FlyCamera a, b;
        a.setYawPitch(0.7f, 0.3f);
        b.setYawPitch(0.7f, 0.3f);
        a.setPosition(vec3(1.0f, 2.0f, 3.0f));
        b.setPosition(vec3(1.0f, 2.0f, 3.0f));
        const vec3 d = a.moveDelta(1.0f, -0.5f, 0.25f, 0.1f, 7.0f);
        a.setPosition(a.position() + d);
        b.move(1.0f, -0.5f, 0.25f, 0.1f, 7.0f);
        CHECK(approxV(a.position(), b.position()), "move equals setPosition(position + moveDelta)");
    }

    // --- 8. Strafing is perpendicular to the look direction, and level. ---
    {
        FlyCamera cam;
        cam.setYawPitch(0.0f, 0.0f);           // facing +X
        cam.setPosition(vec3(0.0f, 0.0f, 0.0f));
        const vec3 right = cam.moveDelta(0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
        CHECK(approxV(right, vec3(0.0f, 0.0f, 1.0f)), "facing +X, strafing right goes +Z");

        // Even with the camera pitched, strafing must stay level or the horizon rolls.
        cam.setYawPitch(0.0f, 1.0f);
        const vec3 pitchedRight = cam.moveDelta(0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
        CHECK(approx(pitchedRight.y, 0.0f), "strafing stays level however the camera is pitched");
        CHECK(approx(glm::length(pitchedRight), 1.0f), "strafe is unit-scaled, not foreshortened");
    }

    // --- 9. Rising uses WORLD up, not the camera's own, so "up" always means up. ---
    {
        FlyCamera cam;
        cam.setYawPitch(2.0f, -1.2f);          // looking down and off to one side
        cam.setPosition(vec3(0.0f, 0.0f, 0.0f));
        const vec3 up = cam.moveDelta(0.0f, 0.0f, 1.0f, 1.0f, 3.0f);
        CHECK(approxV(up, vec3(0.0f, 3.0f, 0.0f)), "up is world up regardless of where the camera looks");
    }

    // --- 10. Standing still is standing still. ---
    {
        FlyCamera cam;
        const vec3 before = cam.position();
        cam.move(0.0f, 0.0f, 0.0f, 0.016f, 6.0f);
        CHECK(approxV(cam.position(), before), "zero input does not drift");
        cam.move(1.0f, 1.0f, 1.0f, 0.0f, 6.0f);
        CHECK(approxV(cam.position(), before), "zero dt does not move either");
    }

    // --- 11. The view matrix is the inverse of where the camera stands. ---
    //     It must send the camera's own position to the origin, and something straight ahead to
    //     negative Z in view space — the convention every shader downstream assumes.
    {
        FlyCamera cam;
        cam.setPosition(vec3(3.0f, -1.0f, 4.0f));
        cam.setYawPitch(0.9f, 0.2f);
        const auto view = cam.view();

        const vec4 eye = view * vec4(cam.position(), 1.0f);
        CHECK(approx(eye.x, 0.0f, 1e-3f) && approx(eye.y, 0.0f, 1e-3f) && approx(eye.z, 0.0f, 1e-3f),
              "the view matrix puts the eye at the origin");

        const vec3 ahead = cam.position() + cam.forward() * 5.0f;
        const vec4 seen = view * vec4(ahead, 1.0f);
        CHECK(seen.z < -4.9f && seen.z > -5.1f, "a point 5 ahead sits 5 down -Z in view space");
        CHECK(approx(seen.x, 0.0f, 1e-3f) && approx(seen.y, 0.0f, 1e-3f), "and dead centre");
    }

    if (g_fail == 0) std::printf("flycamera: all checks passed\n");
    return g_fail == 0 ? 0 : 1;
}
