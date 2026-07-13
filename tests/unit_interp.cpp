// Unit tests for maz::math scalar interpolation, remapping, and easing helpers.
// Pure math, no GPU required.

#include "maz/math/Interpolation.hpp"

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

bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

} // namespace

int main() {
    using math::Easing;

    // --- lerp / inverseLerp / remap ------------------------------------------
    check(approx(math::lerp(0.0f, 10.0f, 0.5f), 5.0f), "lerp midpoint");
    check(approx(math::lerp(2.0f, 4.0f, 0.0f), 2.0f), "lerp at t=0");
    check(approx(math::lerp(2.0f, 4.0f, 1.0f), 4.0f), "lerp at t=1");
    check(approx(math::inverseLerp(0.0f, 10.0f, 2.5f), 0.25f), "inverseLerp");
    check(approx(math::inverseLerp(5.0f, 5.0f, 5.0f), 0.0f), "inverseLerp degenerate guard");
    check(approx(math::remap(5.0f, 0.0f, 10.0f, 0.0f, 100.0f), 50.0f), "remap 0..10 -> 0..100");
    check(approx(math::remap(50.0f, 0.0f, 100.0f, -1.0f, 1.0f), 0.0f), "remap 0..100 -> -1..1");

    // --- saturate ------------------------------------------------------------
    check(approx(math::saturate(-1.0f), 0.0f), "saturate clamps below 0");
    check(approx(math::saturate(2.0f), 1.0f), "saturate clamps above 1");
    check(approx(math::saturate(0.3f), 0.3f), "saturate passes interior");

    // --- smoothstep / smootherstep -------------------------------------------
    check(approx(math::smoothstep(0.0f, 1.0f, 0.5f), 0.5f), "smoothstep midpoint");
    check(approx(math::smoothstep(0.0f, 1.0f, -1.0f), 0.0f), "smoothstep clamps low");
    check(approx(math::smoothstep(0.0f, 1.0f, 2.0f), 1.0f), "smoothstep clamps high");
    check(approx(math::smoothstep(0.0f, 1.0f, 0.25f) + math::smoothstep(0.0f, 1.0f, 0.75f), 1.0f),
          "smoothstep symmetric about 0.5");
    check(approx(math::smootherstep(0.0f, 1.0f, 0.5f), 0.5f), "smootherstep midpoint");
    check(approx(math::smootherstep(0.0f, 1.0f, 0.0f), 0.0f), "smootherstep at 0");
    check(approx(math::smootherstep(0.0f, 1.0f, 1.0f), 1.0f), "smootherstep at 1");

    // --- moveToward ----------------------------------------------------------
    check(approx(math::moveToward(0.0f, 10.0f, 3.0f), 3.0f), "moveToward steps by delta");
    check(approx(math::moveToward(0.0f, 10.0f, 20.0f), 10.0f), "moveToward clamps to target");
    check(approx(math::moveToward(10.0f, 0.0f, 3.0f), 7.0f), "moveToward steps downward");
    check(approx(math::moveToward(5.0f, 5.0f, 1.0f), 5.0f), "moveToward already at target");

    // --- lerpAngle -----------------------------------------------------------
    float r350 = glm::radians(350.0f);
    float r10 = glm::radians(10.0f);
    float m = math::lerpAngle(r350, r10, 0.5f);
    check(approx(std::sin(m), std::sin(0.0f), 1e-4f) && approx(std::cos(m), std::cos(0.0f), 1e-4f),
          "lerpAngle shortest path ~0");
    check(approx(math::lerpAngle(r350, r10, 0.0f), r350), "lerpAngle at t=0 returns a");

    // --- easing --------------------------------------------------------------
    const Easing eases[] = {
        Easing::Linear,   Easing::InQuad,     Easing::OutQuad,   Easing::InOutQuad,
        Easing::InCubic,  Easing::OutCubic,   Easing::InOutCubic, Easing::InSine,
        Easing::OutSine,  Easing::InOutSine};

    for (Easing e : eases) {
        check(approx(math::ease(e, 0.0f), 0.0f, 1e-4f), "easing endpoint at 0");
        check(approx(math::ease(e, 1.0f), 1.0f, 1e-4f), "easing endpoint at 1");
        float prev = math::ease(e, 0.0f);
        bool monotonic = true;
        for (float t = 0.05f; t <= 1.0f + 1e-6f; t += 0.05f) {
            float cur = math::ease(e, t);
            if (cur < prev - 1e-4f) {
                monotonic = false;
            }
            prev = cur;
        }
        check(monotonic, "easing monotonic non-decreasing");
    }

    check(approx(math::ease(Easing::InOutQuad, 0.5f), 0.5f, 1e-4f), "easeInOutQuad midpoint 0.5");
    check(approx(math::ease(Easing::InOutCubic, 0.5f), 0.5f, 1e-4f), "easeInOutCubic midpoint 0.5");
    check(approx(math::ease(Easing::InOutSine, 0.5f), 0.5f, 1e-4f), "easeInOutSine midpoint 0.5");
    check(approx(math::ease(Easing::Linear, 0.37f), 0.37f), "ease Linear is identity");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
