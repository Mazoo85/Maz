// Unit tests for maz::ai::Steering — classic Reynolds steering behaviors
// (seek/flee/arrive/pursue/evade) plus the safeNormalize/truncate helpers.
// Steering forces are float math, so comparisons use an approx tolerance, but the
// inputs are chosen to be exactly representable (axis-aligned unit vectors, 3-4-5
// triangles, halves) so the approx is trivially satisfied. Pursue/evade expecteds
// are computed via the SAME safeNormalize helper so no hand-computed irrational
// constant is asserted. Pure C++, no GPU/display.

#include "maz/ai/Steering.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::ai;
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

} // namespace

int main() {
    // --- 1. SEEK AT REST -----------------------------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = seek(a, vec3(10.0f, 0.0f, 0.0f));
        check(vapprox(s, vec3(1.0f, 0.0f, 0.0f)), "seek at rest -> desired (1,0,0)");
    }

    // --- 2. SEEK TRUNCATED BY maxForce ---------------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.5f};
        vec3 s = seek(a, vec3(10.0f, 0.0f, 0.0f));
        check(vapprox(s, vec3(0.5f, 0.0f, 0.0f)), "seek force capped to maxForce=0.5");
    }

    // --- 3. SEEK WITH VELOCITY (desired == velocity -> zero force) -----------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = seek(a, vec3(10.0f, 0.0f, 0.0f));
        check(vapprox(s, vec3(0.0f, 0.0f, 0.0f)), "seek with matching velocity -> zero force");
    }

    // --- 4. FLEE -------------------------------------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = flee(a, vec3(10.0f, 0.0f, 0.0f));
        check(vapprox(s, vec3(-1.0f, 0.0f, 0.0f)), "flee -> desired away (-1,0,0)");
    }

    // --- 5. ARRIVE INSIDE slowRadius (ramped speed) --------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = arrive(a, vec3(2.0f, 0.0f, 0.0f), 4.0f);
        check(vapprox(s, vec3(0.5f, 0.0f, 0.0f)), "arrive inside slowRadius ramps speed to 0.5");
    }

    // --- 6. ARRIVE OUTSIDE slowRadius (full speed) ---------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = arrive(a, vec3(10.0f, 0.0f, 0.0f), 4.0f);
        check(vapprox(s, vec3(1.0f, 0.0f, 0.0f)), "arrive outside slowRadius -> full speed");
    }

    // --- 7. ARRIVE AT TARGET (brake) -----------------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.3f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 s = arrive(a, vec3(0.0f, 0.0f, 0.0f), 4.0f);
        check(vapprox(s, vec3(-0.3f, 0.0f, 0.0f)), "arrive at target -> brake (-velocity)");
    }

    // --- 8. TRUNCATE HELPER --------------------------------------------------
    {
        vec3 unchanged = truncate(vec3(3.0f, 4.0f, 0.0f), 5.0f);
        check(vapprox(unchanged, vec3(3.0f, 4.0f, 0.0f)), "truncate: len==maxLen unchanged (3,4,0)");
        vec3 scaled = truncate(vec3(3.0f, 4.0f, 0.0f), 2.5f);
        check(vapprox(scaled, vec3(1.5f, 2.0f, 0.0f)), "truncate: len 5 scaled to 2.5 -> (1.5,2,0)");
    }

    // --- 9. safeNormalize ----------------------------------------------------
    {
        vec3 z = safeNormalize(vec3(0.0f, 0.0f, 0.0f));
        check(std::isfinite(z.x) && std::isfinite(z.y) && std::isfinite(z.z),
              "safeNormalize(0) is finite (no NaN)");
        check(vapprox(z, vec3(0.0f, 0.0f, 0.0f)), "safeNormalize(0) -> (0,0,0)");
        vec3 n = safeNormalize(vec3(0.0f, 5.0f, 0.0f));
        check(vapprox(n, vec3(0.0f, 1.0f, 0.0f)), "safeNormalize((0,5,0)) -> (0,1,0)");
    }

    // --- 10. PURSUE (seek predicted future) ----------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 future = vec3(10.0f, 2.0f, 0.0f);  // targetPos(10,0,0) + targetVel(0,1,0)*2
        vec3 s = pursue(a, vec3(10.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), 2.0f);
        check(vapprox(s, safeNormalize(future)), "pursue steers toward predicted future");
        check(approx(glm::length(s), 1.0f), "pursue force is a unit vector (maxSpeed 1, at rest)");
    }

    // --- 11. EVADE (flee predicted future) -----------------------------------
    {
        Agent a{vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), 1.0f, 100.0f};
        vec3 future = vec3(10.0f, 2.0f, 0.0f);
        vec3 s = evade(a, vec3(10.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), 2.0f);
        check(vapprox(s, safeNormalize(-future)), "evade steers away from predicted future");
        check(approx(glm::length(s), 1.0f), "evade force is a unit vector (maxSpeed 1, at rest)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
