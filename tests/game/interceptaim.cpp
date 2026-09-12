// tests/game/interceptaim.cpp — verifies the first-order intercept solver (game::solveIntercept). Ground truths:
// firing along the returned direction at the projectile speed for the returned time lands exactly on the moving
// target (the defining property, checked for several geometries); a stationary target reduces to distance/speed; a
// target fleeing faster than the projectile is unreachable; a non-positive speed and a coincident target are
// handled; the aim point leads a crossing target ahead of its current position. Pure CPU, exact.
#include "maz/game/InterceptAim.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz;

static float dist(const math::vec3& a, const math::vec3& b) {
    const math::vec3 d = a - b;
    return std::sqrt(math::dot(d, d));
}

// The core invariant: a projectile from `shooter` along sol.direction at `speed` for sol.time reaches the
// target's actual position at that time.
static bool intercepts(const math::vec3& shooter, const math::vec3& target, const math::vec3& tvel, float speed) {
    const game::InterceptSolution s = game::solveIntercept(shooter, target, tvel, speed);
    if (!s.hit) return false;
    const math::vec3 projAt = shooter + s.direction * (speed * s.time);
    const math::vec3 tgtAt = target + tvel * s.time;
    return dist(projAt, tgtAt) < 1e-3f;
}

int main() {
    // --- 1. Crossing target: the shot actually intercepts, and the aim leads ahead of the target. ---
    {
        const math::vec3 shooter(0, 0, 0), target(10, 0, 0), tvel(0, 5, 0); // target crosses upward (+Y)
        const game::InterceptSolution s = game::solveIntercept(shooter, target, tvel, 20.0f);
        CHECK(s.hit, "a catchable crossing target is reachable");
        CHECK(intercepts(shooter, target, tvel, 20.0f), "the fired shot lands on the crossing target");
        CHECK(s.aimPoint.y > 0.5f, "aim leads AHEAD of the target's current position (into its path)");
        // direction is unit length.
        CHECK(std::fabs(std::sqrt(math::dot(s.direction, s.direction)) - 1.0f) < 1e-4f, "direction is unit length");
    }

    // --- 2. Stationary target: time == distance / speed and we aim straight at it. ---
    {
        const math::vec3 shooter(0, 0, 0), target(30, 40, 0), tvel(0, 0, 0); // distance 50
        const game::InterceptSolution s = game::solveIntercept(shooter, target, tvel, 25.0f);
        CHECK(s.hit && std::fabs(s.time - 2.0f) < 1e-3f, "stationary target: time = distance/speed (50/25 = 2)");
        CHECK(dist(s.aimPoint, target) < 1e-4f, "aim point is the target itself when it isn't moving");
        CHECK(intercepts(shooter, target, tvel, 25.0f), "the shot lands on a stationary target");
    }

    // --- 3. A target fleeing directly away faster than the projectile is unreachable. ---
    {
        const math::vec3 shooter(0, 0, 0), target(5, 0, 0), tvel(10, 0, 0); // runs away at 10 along +X
        const game::InterceptSolution s = game::solveIntercept(shooter, target, tvel, 8.0f); // slower shot
        CHECK(!s.hit, "a target outrunning the projectile is unreachable");
    }

    // --- 4. Head-on approach is always catchable and lands. ---
    {
        const math::vec3 shooter(0, 0, 0), target(50, 0, 0), tvel(-12, 0, 0); // charging the shooter
        CHECK(intercepts(shooter, target, tvel, 15.0f), "a head-on approaching target is intercepted");
    }

    // --- 5. A fast crossing target still solvable with a fast enough projectile (3D). ---
    {
        const math::vec3 shooter(-20, 5, 3), target(0, 0, 0), tvel(2, 9, -4);
        CHECK(intercepts(shooter, target, tvel, 40.0f), "3D crossing intercept lands with a fast projectile");
    }

    // --- 6. Degenerate inputs. ---
    {
        CHECK(!game::solveIntercept(math::vec3(0, 0, 0), math::vec3(5, 0, 0), math::vec3(0, 0, 0), 0.0f).hit,
              "non-positive projectile speed -> no solution");
        const game::InterceptSolution c = game::solveIntercept(math::vec3(1, 2, 3), math::vec3(1, 2, 3),
                                                               math::vec3(4, 0, 0), 10.0f);
        CHECK(c.hit && c.time < 1e-4f, "a coincident target is an immediate (t=0) hit");
    }

    if (g_fail == 0) {
        std::printf("interceptaim: OK — crossing/stationary/head-on/3D intercepts land, unreachable & degenerate handled.\n");
        return 0;
    }
    std::printf("interceptaim: %d failure(s).\n", g_fail);
    return 1;
}
