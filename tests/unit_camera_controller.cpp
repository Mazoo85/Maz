// Unit tests for maz::scene::OrbitCamera / FlyCamera — camera rigs that compute a Camera's
// placement from intuitive parameters. OrbitCamera orbits a focus at a distance via yaw/pitch
// (pitch clamped away from the pole); FlyCamera is a free-flight eye looking along yaw/pitch
// (forward -Z at zero, camera convention) that moves along its own axes. All float math, so
// comparisons use an approx tolerance, but inputs land on clean 0/±1 trig targets (yaw/pitch of
// 0 or 90°) so the approx is trivially satisfied. Pure C++, no GPU/display.

#include "maz/scene/CameraController.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::scene;
using maz::math::vec3;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }

bool vapprox(vec3 a, vec3 b) {
    return approx(a.x, b.x) && approx(a.y, b.y) && approx(a.z, b.z);
}

const float HALF_PI = 1.57079632679f;

} // namespace

int main() {
    // --- 1. OrbitCamera default position --------------------------------------
    {
        OrbitCamera o;
        check(vapprox(o.position(), vec3(0.0f, 0.0f, 5.0f)),
              "orbit default -> focus + distance*(0,0,1) = (0,0,5)");
    }

    // --- 2. OrbitCamera yaw 90 ------------------------------------------------
    {
        OrbitCamera o;
        o.yaw = HALF_PI;
        check(vapprox(o.position(), vec3(5.0f, 0.0f, 0.0f)),
              "orbit yaw=90 -> (5,0,0)");
    }

    // --- 3. OrbitCamera pitch 90-ish (raw field, bypassing clamp) -------------
    {
        OrbitCamera o;
        o.pitch = HALF_PI;
        check(vapprox(o.position(), vec3(0.0f, 5.0f, 0.0f)),
              "orbit pitch=90 (raw) -> (0,5,0)");
    }

    // --- 4. OrbitCamera orbit clamps pitch, yaw wraps freely ------------------
    {
        OrbitCamera o;
        o.orbit(0.0f, 10.0f);
        check(approx(o.pitch, o.maxPitch), "orbit clamps pitch to maxPitch (not 10)");
        o.orbit(0.0f, -10.0f);
        check(approx(o.pitch, o.minPitch), "orbit clamps pitch to minPitch");
        OrbitCamera y;
        y.orbit(3.0f, 0.0f);
        y.orbit(3.0f, 0.0f);
        check(approx(y.yaw, 6.0f), "orbit yaw wraps freely -> 6.0 (no clamp)");
    }

    // --- 5. OrbitCamera zoom clamps -------------------------------------------
    {
        OrbitCamera o;
        o.zoom(-100.0f);
        check(approx(o.distance, o.minDistance), "zoom clamps distance to minDistance (0.1)");
        OrbitCamera o2;
        o2.zoom(100000.0f);
        check(approx(o2.distance, o2.maxDistance), "zoom clamps distance to maxDistance");
    }

    // --- 6. OrbitCamera applyTo -----------------------------------------------
    {
        OrbitCamera o;
        o.yaw = HALF_PI;
        Camera cam;
        o.applyTo(cam);
        check(vapprox(cam.position(), vec3(5.0f, 0.0f, 0.0f)) &&
              vapprox(cam.target(), vec3(0.0f, 0.0f, 0.0f)),
              "orbit applyTo sets camera position (5,0,0) + target focus (0,0,0)");
    }

    // --- 7. FlyCamera forward -------------------------------------------------
    {
        FlyCamera f;
        check(vapprox(f.forward(), vec3(0.0f, 0.0f, -1.0f)), "fly forward at zero -> (0,0,-1)");
        f.yaw = HALF_PI;
        check(vapprox(f.forward(), vec3(1.0f, 0.0f, 0.0f)), "fly yaw=90 -> forward (1,0,0)");
        FlyCamera g;
        g.pitch = HALF_PI;
        check(vapprox(g.forward(), vec3(0.0f, 1.0f, 0.0f)), "fly pitch=90 (raw) -> forward (0,1,0)");
    }

    // --- 8. FlyCamera right orthogonal + unit ---------------------------------
    {
        FlyCamera f; // yaw0 -> forward (0,0,-1)
        check(vapprox(f.right(), vec3(1.0f, 0.0f, 0.0f)), "fly right at zero -> +X (1,0,0)");
        check(approx(maz::math::dot(f.right(), f.forward()), 0.0f), "fly right orthogonal to forward");
        check(approx(glm::length(f.right()), 1.0f), "fly right is unit length");
    }

    // --- 9. FlyCamera moveLocal -----------------------------------------------
    {
        FlyCamera f; // yaw0, forward (0,0,-1)
        f.moveLocal(2.0f, 0.0f, 0.0f);
        check(vapprox(f.position, vec3(0.0f, 0.0f, -2.0f)), "moveLocal forward 2 -> (0,0,-2)");
        f.moveLocal(0.0f, 1.0f, 0.0f);
        check(vapprox(f.position, vec3(1.0f, 0.0f, -2.0f)), "moveLocal right 1 -> (1,0,-2)");
        f.moveLocal(0.0f, 0.0f, 3.0f);
        check(vapprox(f.position, vec3(1.0f, 3.0f, -2.0f)), "moveLocal up 3 -> (1,3,-2)");
    }

    // --- 10. FlyCamera look clamp + applyTo -----------------------------------
    {
        FlyCamera f;
        f.look(0.0f, 10.0f);
        check(approx(f.pitch, f.maxPitch), "look clamps pitch to maxPitch");
        FlyCamera g;
        Camera cam;
        g.applyTo(cam);
        check(vapprox(cam.target(), cam.position() + g.forward()),
              "fly applyTo target = position + forward (-> (0,0,-1))");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
