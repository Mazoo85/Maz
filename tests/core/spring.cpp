// tests/core/spring.cpp — verifies the damped-spring smoother (core::Spring). Ground truths: a spring converges to
// its target and comes to rest; an under-damped spring (low ζ) overshoots the target at least once; critical/
// over-damped springs (ζ>=1) never overshoot; a spring already at rest on its target stays put; motion is
// deterministic; and dt<=0 or a non-positive frequency is a no-op. Physical behaviour checked by simulation. Pure CPU.
#include "maz/core/Spring.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::core;

// Simulate `seconds` at 1/120 s steps; return the maximum position reached.
static float simulate(Spring& s, float seconds) {
    float maxPos = s.position;
    const float dt = 1.0f / 120.0f;
    for (float t = 0.0f; t < seconds; t += dt) {
        s.update(dt);
        if (s.position > maxPos) maxPos = s.position;
    }
    return maxPos;
}

int main() {
    // --- 1. Converges to target and comes to rest (critical). ---
    {
        Spring s(2.0f, 1.0f);
        s.reset(0.0f);
        s.target = 10.0f;
        simulate(s, 6.0f);
        CHECK(std::fabs(s.position - 10.0f) < 0.05f && std::fabs(s.velocity) < 0.05f, "critical spring settles on target");
        CHECK(s.atRest(0.1f, 0.1f), "atRest() reports settled");
    }

    // --- 2. Under-damped overshoots the target. ---
    {
        Spring s(2.0f, 0.15f); // bouncy
        s.reset(0.0f);
        s.target = 10.0f;
        const float maxPos = simulate(s, 6.0f);
        CHECK(maxPos > 10.5f, "an under-damped spring overshoots its target");
        CHECK(std::fabs(s.position - 10.0f) < 0.5f, "it still settles near the target eventually");
    }

    // --- 3. Critical and over-damped never overshoot. ---
    {
        Spring crit(2.0f, 1.0f);
        crit.reset(0.0f); crit.target = 10.0f;
        CHECK(simulate(crit, 6.0f) <= 10.05f, "critical spring does not overshoot");
        Spring over(2.0f, 2.5f);
        over.reset(0.0f); over.target = 10.0f;
        CHECK(simulate(over, 8.0f) <= 10.05f, "over-damped spring does not overshoot");
    }

    // --- 4. At rest on target stays put; no-ops. ---
    {
        Spring s(3.0f, 0.5f);
        s.reset(5.0f); s.target = 5.0f;
        for (int i = 0; i < 100; ++i) s.update(1.0f / 60.0f);
        CHECK(std::fabs(s.position - 5.0f) < 1e-4f && std::fabs(s.velocity) < 1e-4f, "resting on target stays put");
        Spring n(2.0f, 1.0f); n.reset(0.0f); n.target = 10.0f;
        n.update(0.0f);
        CHECK(n.position == 0.0f, "dt<=0 is a no-op");
        Spring z(0.0f, 1.0f); z.reset(0.0f); z.target = 10.0f;
        z.update(0.1f);
        CHECK(z.position == 0.0f, "non-positive frequency is a no-op");
    }

    // --- 5. Deterministic. ---
    {
        Spring a(2.5f, 0.4f), b(2.5f, 0.4f);
        a.reset(1.0f); b.reset(1.0f); a.target = 7.0f; b.target = 7.0f;
        for (int i = 0; i < 50; ++i) { a.update(1.0f / 90.0f); b.update(1.0f / 90.0f); }
        CHECK(a.position == b.position && a.velocity == b.velocity, "identical springs evolve identically");
    }

    if (g_fail == 0) {
        std::printf("spring: OK — converges/rests, under-damped overshoots, critical/over don't, no-ops, deterministic.\n");
        return 0;
    }
    std::printf("spring: %d failure(s).\n", g_fail);
    return 1;
}
