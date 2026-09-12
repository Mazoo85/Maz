// tests/math/damp.cpp — verifies frame-rate-independent exponential smoothing (math::damp / dampAngle).
// The defining property is composition: smoothing once over dt must equal smoothing over two sub-steps
// of dt/2 (exactly, because 2^(-dt/h) = (2^(-(dt/2)/h))^2) — the very property the tempting
// lerp(x,target,alpha) reimplementation FAILS, which we also assert to prove damp is not just a lerp.
// Ground truths are hand-computed: after one half-life the gap halves, after two it quarters; halfLife<=0
// snaps; dt<=0 is a no-op; dampAngle takes the short way around the circle. Pure CPU, deterministic.
#include "maz/math/Damp.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec2;
using maz::math::vec3;
using maz::math::damp;
using maz::math::dampAngle;
using maz::math::dampFactor;

static bool near1(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

int main() {
    // --- 1. After exactly one half-life the remaining gap is halved. ---
    {
        const float r = damp(0.0f, 10.0f, /*halfLife*/0.5f, /*dt*/0.5f);
        CHECK(near1(r, 5.0f), "one half-life halves the gap"); // 10 + (0-10)*2^-1
    }

    // --- 2. After two half-lives the gap is quartered. ---
    {
        const float r = damp(0.0f, 10.0f, 0.5f, 1.0f);
        CHECK(near1(r, 7.5f), "two half-lives quarter the gap"); // 10 + (-10)*2^-2
    }

    // --- 3. halfLife <= 0 snaps straight to target; dt <= 0 leaves current untouched. ---
    {
        CHECK(near1(damp(3.0f, 9.0f, 0.0f, 0.016f), 9.0f), "zero half-life snaps to target");
        CHECK(near1(damp(3.0f, 9.0f, -1.0f, 0.016f), 9.0f), "negative half-life snaps to target");
        CHECK(near1(damp(3.0f, 9.0f, 0.25f, 0.0f), 3.0f), "zero dt is a no-op");
        CHECK(near1(damp(3.0f, 9.0f, 0.25f, -0.5f), 3.0f), "negative dt is a no-op");
    }

    // --- 4. Frame-rate independence: one big step == many small sub-steps (the whole point). ---
    {
        const float halfLife = 0.3f, dt = 0.2f, target = 42.0f;
        const float once = damp(1.0f, target, halfLife, dt);
        float many = 1.0f;
        const int n = 8;
        for (int i = 0; i < n; ++i) many = damp(many, target, halfLife, dt / static_cast<float>(n));
        CHECK(near1(once, many, 1e-3f), "damp is frame-rate independent (one step == n sub-steps)");

        // And prove it is NOT a naive lerp: the same split with a fixed lerp alpha diverges noticeably.
        const float alpha = 0.5f;
        const float lerpOnce = 1.0f + (target - 1.0f) * alpha;
        float lerpMany = 1.0f;
        for (int i = 0; i < n; ++i) lerpMany = lerpMany + (target - lerpMany) * (alpha / static_cast<float>(n));
        CHECK(std::fabs(lerpOnce - lerpMany) > 1.0f, "naive lerp is frame-rate DEPENDENT (control)");
    }

    // --- 5. Always converges toward target and never overshoots, even for a huge dt. ---
    {
        float v = 0.0f;
        float prev = v;
        for (int i = 0; i < 200; ++i) {
            v = damp(v, 1.0f, 0.1f, 0.05f);
            CHECK(v >= prev - 1e-6f && v <= 1.0f + 1e-6f, "monotone, no overshoot");
            prev = v;
        }
        CHECK(near1(v, 1.0f, 1e-3f), "converges to target");
        // A single enormous dt collapses to (near) the target without blowing past it.
        CHECK(near1(damp(0.0f, 1.0f, 0.1f, 1e6f), 1.0f), "huge dt lands on target, no overshoot");
    }

    // --- 6. Vector overloads decay each component with the shared factor. ---
    {
        const float k = dampFactor(0.5f, 0.5f); // 0.5
        const vec2 r2 = damp(vec2(0.0f, 4.0f), vec2(8.0f, 8.0f), 0.5f, 0.5f);
        CHECK(near1(r2.x, 8.0f + (0.0f - 8.0f) * k) && near1(r2.y, 8.0f + (4.0f - 8.0f) * k),
              "vec2 damp is component-wise");
        const vec3 r3 = damp(vec3(0.0f, 0.0f, 10.0f), vec3(2.0f, 2.0f, 2.0f), 0.5f, 0.5f);
        CHECK(near1(r3.x, 1.0f) && near1(r3.y, 1.0f) && near1(r3.z, 6.0f), "vec3 damp is component-wise");
    }

    // --- 7. dampAngle takes the short way across the 0/2pi seam. ---
    {
        const float deg = 3.14159265358979f / 180.0f;
        // 350deg toward 10deg: shortest delta is +20deg forward. After one half-life move +10deg -> 360deg.
        const float r = dampAngle(350.0f * deg, 10.0f * deg, 0.5f, 0.5f);
        CHECK(near1(r, 360.0f * deg, 1e-3f), "dampAngle crosses the seam the short way");
        // With no wraparound in play, dampAngle matches plain damp.
        CHECK(near1(dampAngle(0.2f, 0.8f, 0.3f, 0.1f), damp(0.2f, 0.8f, 0.3f, 0.1f), 1e-5f),
              "dampAngle == damp when no wrap is needed");
    }

    if (g_fail == 0) std::printf("damp: all tests passed\n");
    return g_fail == 0 ? 0 : 1;
}
