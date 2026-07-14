// Unit tests for maz::math::smoothDamp — the critically-damped spring smoother
// (exact Unity Mathf.SmoothDamp transcription). SmoothDamp is an approximation,
// so these tests are PROPERTY-BASED (convergence, monotonic no-overshoot,
// per-step direction, at-rest stationarity, maxSpeed clamping, smoothTime=0
// safety, downward mirror, and vec3 per-component independence) rather than
// asserting exact numeric outputs. Pure C++, no GPU/display.

#include "maz/math/SmoothDamp.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) < tol; }

// Runs N smoothDamp steps, feeding the output back as current and persisting vel.
float run(float from, float to, float st, float dt, int steps, float& vel) {
    float current = from;
    for (int i = 0; i < steps; ++i) {
        current = smoothDamp(current, to, vel, st, dt);
    }
    return current;
}

} // namespace

int main() {
    const float dt = 1.0f / 60.0f;

    // --- 1. CONVERGES TOWARD TARGET ------------------------------------------
    {
        float vel = 0.0f;
        float current = run(0.0f, 10.0f, 0.3f, dt, 200, vel);
        check(approx(current, 10.0f, 1e-2f), "converges: current -> 10 after 200 steps");
        check(std::fabs(vel) < 1e-2f, "converges: velocity settles to ~0 at rest");
    }

    // --- 2. MONOTONIC, NO OVERSHOOT ------------------------------------------
    {
        float vel = 0.0f;
        float current = 0.0f;
        float prev = 0.0f;
        bool ok = true;
        for (int i = 0; i < 200; ++i) {
            current = smoothDamp(current, 10.0f, vel, 0.3f, dt);
            if (current < prev - 1e-5f || current > 10.0f + 1e-4f) {
                ok = false;
            }
            prev = current;
        }
        check(ok, "monotonic non-decreasing and never overshoots above 10");
    }

    // --- 3. SINGLE STEP MOVES TOWARD TARGET ----------------------------------
    {
        float vel = 0.0f;
        float out = smoothDamp(0.0f, 10.0f, vel, 0.3f, dt);
        check(out > 0.0f && out < 10.0f, "single step moves partway toward target (0 < out < 10)");
        check(vel > 0.0f, "single step velocity is positive (moving in +direction)");
    }

    // --- 4. ALREADY AT TARGET STAYS ------------------------------------------
    {
        float vel = 0.0f;
        float out = smoothDamp(5.0f, 5.0f, vel, 0.3f, dt);
        check(approx(out, 5.0f), "already at target -> stays at 5");
        check(approx(vel, 0.0f), "already at target -> velocity stays ~0");
    }

    // --- 5. maxSpeed CLAMPS APPROACH -----------------------------------------
    {
        float vel = 0.0f;
        const float smoothTime = 0.3f;
        const float maxSpeed = 1.0f;
        float out = smoothDamp(0.0f, 1000.0f, vel, smoothTime, dt, maxSpeed);
        // A huge unclamped step doesn't happen: the per-step change is bounded.
        check(out < 0.0f + maxSpeed * smoothTime * 2.0f, "maxSpeed bounds the per-step change");
    }

    // --- 6. smoothTime CLAMP (0 IS SAFE) -------------------------------------
    {
        float vel = 0.0f;
        float out = smoothDamp(0.0f, 10.0f, vel, 0.0f, dt);
        check(std::isfinite(out), "smoothTime=0 output is finite (no div-by-zero / NaN)");
        check(out > 0.9f * 10.0f, "smoothTime=0 near-snaps strongly toward target");
    }

    // --- 7. NEGATIVE DIRECTION -----------------------------------------------
    {
        float vel = 0.0f;
        float current = 10.0f;
        float prev = 10.0f;
        bool ok = true;
        bool sawNegVel = false;
        for (int i = 0; i < 200; ++i) {
            current = smoothDamp(current, 0.0f, vel, 0.3f, dt);
            if (current > prev + 1e-5f || current < 0.0f - 1e-4f) {
                ok = false;
            }
            if (vel < 0.0f) {
                sawNegVel = true;
            }
            prev = current;
        }
        check(ok, "negative dir: monotonic non-increasing, stays in [0,10]");
        check(sawNegVel, "negative dir: velocity negative during approach");
        check(approx(current, 0.0f, 1e-2f), "negative dir: converges down to ~0");
    }

    // --- 8. vec3 PER-COMPONENT -----------------------------------------------
    {
        vec3 current(0.0f, 0.0f, 0.0f);
        const vec3 target(10.0f, -5.0f, 3.0f);
        vec3 vel(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 200; ++i) {
            current = smoothDamp(current, target, vel, 0.3f, dt);
        }
        check(approx(current.x, 10.0f, 1e-2f) && approx(current.y, -5.0f, 1e-2f) &&
                  approx(current.z, 3.0f, 1e-2f),
              "vec3 per-axis converges to (10,-5,3)");
        check(std::fabs(vel.x) < 1e-2f && std::fabs(vel.y) < 1e-2f && std::fabs(vel.z) < 1e-2f,
              "vec3 velocity settles to ~0 on every axis");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
