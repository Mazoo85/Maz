// Unit tests for maz::scene::Camera — pure matrix math, no GPU required.
// Verifies the view/projection follow the engine's Vulkan conventions.

#include "maz/scene/Camera.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// World point -> clip space (before perspective divide).
math::vec4 clip(const scene::Camera& cam, math::vec3 p) {
    return cam.viewProjection() * math::vec4(p, 1.0f);
}

} // namespace

int main() {
    scene::Camera cam;
    cam.setPosition({0.0f, 0.0f, 3.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});
    cam.setPerspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    const math::vec3 f = cam.forward();
    check(std::fabs(f.x) < 1e-5f && std::fabs(f.y) < 1e-5f && f.z < -0.99f,
          "forward points down -Z");

    const math::vec4 c = clip(cam, {0.0f, 0.0f, 0.0f});
    check(c.w > 0.0f, "origin is in front of camera (w>0)");
    check(std::fabs(c.x) < 1e-4f && std::fabs(c.y) < 1e-4f, "origin maps to screen center");
    const float ndcZ = c.z / c.w;
    check(ndcZ > 0.0f && ndcZ < 1.0f, "origin depth within Vulkan 0..1 range");

    const math::vec4 r = clip(cam, {0.5f, 0.0f, 0.0f});
    check((r.x / r.w) > 0.0f, "world +X -> screen +X");

    const math::vec4 u = clip(cam, {0.0f, 0.5f, 0.0f});
    check((u.y / u.w) < 0.0f, "world +Y -> screen -Y (Vulkan Y-flip)");

    const math::vec4 near = clip(cam, {0.0f, 0.0f, 2.0f});  // closer to camera at z=3
    const math::vec4 far = clip(cam, {0.0f, 0.0f, -5.0f});  // farther away
    check((near.z / near.w) < (far.z / far.w), "closer point has smaller depth");

    cam.setAspect(1.0f);
    check(std::fabs(cam.aspect() - 1.0f) < 1e-6f, "setAspect updates aspect");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
