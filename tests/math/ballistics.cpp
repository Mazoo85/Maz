// tests/math/ballistics.cpp — verifies the ballistics aiming module (math Ballistics.hpp).
// Ground truths, deterministic (fixed + seeded-LCG shots, no <random>, no clock):
//   * LAUNCH HITS (airtight): firing at each solved angle and INDEPENDENTLY integrating the projectile under
//     gravity (semi-implicit Euler, fine step) makes it pass through the target — validates the closed form;
//   * MAX RANGE: a ground target at exactly s^2/g needs a single 45-degree shot; low+high angles for a
//     level target sum to 90 degrees;
//   * OUT OF RANGE: a target past the reach yields no solution;
//   * 3D: launch velocities have the requested speed and, integrated under gravity, reach the target;
//   * INTERCEPT (airtight): at the solved time the target sits exactly a projectile-flight away from the
//     shooter (|aim - shooter| == projectileSpeed * time), and the target really is at the aim point.
#include "maz/math/Ballistics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

// Independent 2D simulation: fire from origin at angle theta and speed s under gravity g; return the height
// when the projectile's horizontal position first reaches targetX (or 1e30 if it never does going forward).
static float simulateHeightAtX(float speed, float theta, float gravity, float targetX) {
    float x = 0.0f, y = 0.0f;
    const float vx = speed * std::cos(theta);
    float vy = speed * std::sin(theta);
    const float dt = 1e-3f;
    // Exact constant-acceleration step (position is quadratic in dt) — no integration drift, yet still an
    // independent forward integrator that never calls the solver.
    for (int i = 0; i < 2000000 && x <= targetX + 1.0f; ++i) {
        const float px = x, py = y;
        x += vx * dt;
        y += vy * dt - 0.5f * gravity * dt * dt;
        vy -= gravity * dt;
        if (px <= targetX && x >= targetX) {
            const float f = (targetX - px) / (x - px + 1e-20f);
            return py + (y - py) * f;
        }
        if (vy < 0.0f && y < -2000.0f) {
            break;
        }
    }
    return 1e30f;
}

int main() {
    using namespace maz::math;

    // --- 1. Solved angles actually hit the target (independent gravity simulation). ---
    {
        Lcg rng{0xBA11u};
        float worst = 0.0f;
        int hits = 0;
        for (int i = 0; i < 250; ++i) {
            const float speed = 15.0f + rng.unit() * 25.0f;
            const float g = 6.0f + rng.unit() * 8.0f;
            const float tx = 2.0f + rng.unit() * 20.0f;
            const float ty = -6.0f + rng.unit() * 12.0f;
            const LaunchAngles a = ballisticAngles(speed, tx, ty, g);
            if (a.count == 0) {
                continue;
            }
            const float yl = simulateHeightAtX(speed, a.low, g, tx);
            worst = std::max(worst, std::fabs(yl - ty));
            ++hits;
            if (a.count == 2) {
                const float yh = simulateHeightAtX(speed, a.high, g, tx);
                worst = std::max(worst, std::fabs(yh - ty));
            }
        }
        CHECK(hits > 100, "many random shots are in range and solved");
        CHECK(worst < 3e-2f, "firing at the solved angle(s) lands the projectile on the target");
    }

    // --- 2. Max range + complementary angles for a level target. ---
    {
        const float s = 20.0f, g = 9.8f;
        const float R = s * s / g; // level max range
        const LaunchAngles atMax = ballisticAngles(s, R, 0.0f, g);
        CHECK(atMax.count == 1, "a target at max range has a single solution");
        CHECK(std::fabs(atMax.low - 0.785398163f) < 2e-3f, "the max-range angle is 45 degrees");
        const LaunchAngles half = ballisticAngles(s, R * 0.6f, 0.0f, g);
        CHECK(half.count == 2, "a level target inside range has two arcs");
        CHECK(std::fabs((half.low + half.high) - 1.57079632679f) < 2e-3f,
              "the low and high angles for a level target sum to 90 degrees");
        const LaunchAngles tooFar = ballisticAngles(s, R * 1.2f, 0.0f, g);
        CHECK(tooFar.count == 0, "a target beyond max range is unreachable");
    }

    // --- 3. 3D velocities: correct speed, and integrated under gravity they reach the target. ---
    {
        const vec3 from(1, 2, -3), to(9, 4, 5);
        const float speed = 18.0f, g = 9.8f;
        const LaunchVelocities v = ballisticVelocities(from, to, speed, g);
        CHECK(v.count >= 1, "the 3D shot is solvable");
        if (v.count >= 1) {
            CHECK(std::fabs(len(v.low) - speed) < 1e-3f, "the low-arc launch velocity has the requested speed");
            // Integrate under gravity and check closest approach to the target.
            auto reaches = [&](const vec3& v0) {
                vec3 p = from, vel = v0;
                const float dt = 1e-3f;
                float best = 1e30f;
                for (int i = 0; i < 400000; ++i) {
                    // Exact constant-acceleration step (drift-free).
                    p.x += vel.x * dt;
                    p.z += vel.z * dt;
                    p.y += vel.y * dt - 0.5f * g * dt * dt;
                    vel.y -= g * dt;
                    best = std::min(best, len(p - to));
                    if (p.y < to.y - 50.0f) {
                        break;
                    }
                }
                return best;
            };
            CHECK(reaches(v.low) < 5e-2f, "the low-arc shot flies through the target");
            if (v.count == 2) {
                CHECK(std::fabs(len(v.high) - speed) < 1e-3f, "the high-arc velocity has the requested speed");
                CHECK(reaches(v.high) < 5e-2f, "the high-arc shot flies through the target");
            }
        }
    }

    // --- 4. Intercept lead: airtight geometric identities. ---
    {
        Lcg rng{0x1EADu};
        int solved = 0;
        float worstFlight = 0.0f, worstTarget = 0.0f;
        for (int i = 0; i < 3000; ++i) {
            const vec3 shooter(rng.unit() * 4.0f, rng.unit() * 4.0f, rng.unit() * 4.0f);
            const vec3 tp(rng.unit() * 20.0f - 10.0f, rng.unit() * 20.0f - 10.0f, rng.unit() * 20.0f - 10.0f);
            const vec3 tv(rng.unit() * 6.0f - 3.0f, rng.unit() * 6.0f - 3.0f, rng.unit() * 6.0f - 3.0f);
            const float ps = 8.0f + rng.unit() * 12.0f;
            const Intercept it = interceptLead(shooter, ps, tp, tv);
            if (!it.ok) {
                continue;
            }
            ++solved;
            // The projectile, flying at ps for `time`, covers exactly the distance to the aim point.
            worstFlight = std::max(worstFlight, std::fabs(len(it.aimPoint - shooter) - ps * it.time));
            // The target really is at the aim point at that time.
            worstTarget = std::max(worstTarget, len((tp + tv * it.time) - it.aimPoint));
            CHECK(std::fabs(len(it.direction) - 1.0f) < 1e-4f, "the fire direction is unit length");
        }
        CHECK(solved > 1500, "many intercepts are solvable");
        CHECK(worstFlight < 2e-3f, "the projectile flight distance matches speed * time to the aim point");
        CHECK(worstTarget < 2e-3f, "the aim point is exactly where the target will be");
    }

    if (g_fail == 0) {
        std::printf("ballistics: OK — launch hits, max range, out-of-range, 3D, intercept lead.\n");
        return 0;
    }
    std::printf("ballistics: %d failure(s).\n", g_fail);
    return 1;
}
