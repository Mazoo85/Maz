#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

// maz::game INTERCEPT AIM — first-order "lead the target" solver: where should a turret, archer, or homing AI
// aim so a projectile fired at a FIXED speed hits a target that is moving at constant velocity? This is the
// gravity-free companion to `Ballistics.hpp` (which arcs a shot under gravity) — the math a tower-defense tower,
// a spaceship gun, or a guard's thrown rock needs to hit a mover. Closed-form (one quadratic), exact,
// deterministic, header-only. Works in 3D; for a 2D game leave z = 0.
namespace maz::game {

// Result of an intercept solve.
struct InterceptSolution {
    bool hit = false;          // false when the target simply cannot be caught at this projectile speed
    float time = 0.0f;         // seconds until impact
    math::vec3 aimPoint{0, 0, 0};   // the future target position to aim at
    math::vec3 direction{0, 0, 0};  // unit direction to fire (zero if degenerate)
};

namespace detail {
// Smallest strictly-positive root of a*t^2 + b*t + c = 0, or -1 if there is none.
inline float smallestPositiveRoot(float a, float b, float c) {
    constexpr float eps = 1e-6f;
    if (std::fabs(a) < eps) {                 // linear: b*t + c = 0
        if (std::fabs(b) < eps) return -1.0f; // no (finite) solution
        const float t = -c / b;
        return t > eps ? t : -1.0f;
    }
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return -1.0f;
    const float sq = std::sqrt(disc);
    const float t0 = (-b - sq) / (2.0f * a);
    const float t1 = (-b + sq) / (2.0f * a);
    float lo = t0 < t1 ? t0 : t1;
    float hi = t0 < t1 ? t1 : t0;
    if (lo > eps) return lo;
    if (hi > eps) return hi;
    return -1.0f;
}
} // namespace detail

// Aim a `projectileSpeed` shot from `shooter` to intercept a target now at `target` moving at constant
// `targetVel`. Returns the impact time, the point to aim at, and the unit fire direction. `hit` is false
// when the target outruns the projectile (no positive-time solution) or the projectile speed is non-positive.
inline InterceptSolution solveIntercept(const math::vec3& shooter, const math::vec3& target,
                                        const math::vec3& targetVel, float projectileSpeed) {
    InterceptSolution s;
    if (projectileSpeed <= 0.0f) return s;

    const math::vec3 d = target - shooter;              // relative position
    // |d + targetVel*t| = projectileSpeed*t  ->  (|vt|^2 - vp^2) t^2 + 2(d·vt) t + |d|^2 = 0
    const float a = math::dot(targetVel, targetVel) - projectileSpeed * projectileSpeed;
    const float b = 2.0f * math::dot(d, targetVel);
    const float c = math::dot(d, d);

    const float t = detail::smallestPositiveRoot(a, b, c);
    if (t < 0.0f) {
        // Special case: already coincident -> a 0-time "hit" with no meaningful direction.
        if (c < 1e-12f) { s.hit = true; s.aimPoint = target; }
        return s;
    }
    s.hit = true;
    s.time = t;
    s.aimPoint = target + targetVel * t;
    const math::vec3 to = s.aimPoint - shooter;
    const float len = std::sqrt(math::dot(to, to));
    if (len > 1e-9f) s.direction = to / len;
    return s;
}

} // namespace maz::game
