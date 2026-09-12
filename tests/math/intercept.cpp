// tests/math/intercept.cpp — verifies the projectile lead/intercept solver (math::interceptTarget).
// Ground truths, all closed-form float math, deterministic:
//   * a STATIONARY target is aimed at directly, with time = distance / projectile speed;
//   * the DEFINING invariant for any hit: distance(shooter, aimPoint) == projSpeed * time — the
//     projectile reaches the lead point exactly when the target does (checked in 2D and 3D, for
//     perpendicular and closing motion, including the a==0 linear case where target speed == proj speed);
//   * a target moving directly AWAY faster than the projectile cannot be hit (hit=false);
//   * the returned direction is a unit vector pointing at the aim point;
//   * the 3D solver agrees with the same invariant.
#include "maz/math/Intercept.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Intercept2D;
using maz::math::Intercept3D;
using maz::math::interceptTarget;
using maz::math::vec2;
using maz::math::vec3;

static float len2(vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static float len3(vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    // --- 1. Stationary target: aim straight at it. ---
    {
        const Intercept2D r = interceptTarget(vec2(0, 0), vec2(10, 0), vec2(0, 0), 5.0f);
        CHECK(r.hit, "stationary target is hittable");
        CHECK(std::fabs(r.time - 2.0f) < 1e-4f, "time = distance / speed = 10/5 = 2");
        CHECK(std::fabs(r.aimPoint.x - 10.0f) < 1e-4f && std::fabs(r.aimPoint.y) < 1e-4f,
              "aim point is the target");
        CHECK(std::fabs(r.direction.x - 1.0f) < 1e-4f && std::fabs(r.direction.y) < 1e-4f,
              "direction points straight at it");
    }

    // --- 2. Perpendicular motion: lead the target; invariant holds. ---
    {
        const vec2 shooter(0, 0), target(10, 0), vel(0, 4);
        const float speed = 6.0f;
        const Intercept2D r = interceptTarget(shooter, target, vel, speed);
        CHECK(r.hit, "perpendicular target hittable at this speed");
        CHECK(r.aimPoint.y > 0.0f, "aim leads ahead of the target (positive y)");
        // Defining invariant: the projectile travels speed*time to reach the aim point.
        const float dist = len2(vec2(r.aimPoint.x - shooter.x, r.aimPoint.y - shooter.y));
        CHECK(std::fabs(dist - speed * r.time) < 1e-3f, "distance to aim == speed * time");
        // And the target actually reaches the aim point at that time.
        const vec2 targetAt(target.x + vel.x * r.time, target.y + vel.y * r.time);
        CHECK(std::fabs(targetAt.x - r.aimPoint.x) < 1e-4f &&
                  std::fabs(targetAt.y - r.aimPoint.y) < 1e-4f,
              "target reaches the aim point at intercept time");
        CHECK(std::fabs(len2(r.direction) - 1.0f) < 1e-4f, "direction is unit length");
    }

    // --- 3. Linear (a==0) case: target speed == projectile speed, closing. ---
    {
        // target at (10,0) moving (-3,4) (speed 5), projectile speed 5 -> a = 0 branch.
        const vec2 shooter(0, 0), target(10, 0), vel(-3, 4);
        const float speed = 5.0f;
        const Intercept2D r = interceptTarget(shooter, target, vel, speed);
        CHECK(r.hit, "equal-speed closing target is hittable (linear case)");
        const float dist = len2(vec2(r.aimPoint.x, r.aimPoint.y));
        CHECK(std::fabs(dist - speed * r.time) < 1e-3f, "linear-case invariant holds");
    }

    // --- 4. Target outruns the projectile -> no solution. ---
    {
        const Intercept2D r = interceptTarget(vec2(0, 0), vec2(10, 0), vec2(10, 0), 5.0f);
        CHECK(!r.hit, "target fleeing faster than the projectile cannot be hit");
    }

    // --- 5. 3D intercept invariant. ---
    {
        const vec3 shooter(0, 0, 0), target(8, 3, -2), vel(0, 0, 5);
        const float speed = 9.0f;
        const Intercept3D r = interceptTarget(shooter, target, vel, speed);
        CHECK(r.hit, "3D target hittable");
        const float dist = len3(vec3(r.aimPoint.x, r.aimPoint.y, r.aimPoint.z));
        CHECK(std::fabs(dist - speed * r.time) < 1e-3f, "3D distance to aim == speed * time");
        const vec3 targetAt(target.x + vel.x * r.time, target.y + vel.y * r.time,
                            target.z + vel.z * r.time);
        CHECK(std::fabs(targetAt.z - r.aimPoint.z) < 1e-4f, "3D target reaches the aim point");
        CHECK(std::fabs(len3(r.direction) - 1.0f) < 1e-4f, "3D direction is unit length");
    }

    if (g_fail == 0) {
        std::printf("intercept: OK — stationary, lead invariant (2D/3D), linear case, unreachable, "
                    "unit direction.\n");
        return 0;
    }
    std::printf("intercept: %d failure(s).\n", g_fail);
    return 1;
}
