#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

// maz::game ballistics — closed-form projectile-motion helpers for aiming under gravity: the math an AI
// (or the player's aim assist) needs to lob a grenade, arc an arrow, or range an artillery shell onto a
// target. Godot ships no ballistic solver, so this is a beyond-Godot gameplay utility. All functions are
// exact/analytic (no iteration), deterministic, header-only. Gravity is given as a positive magnitude
// acting along -Y.
namespace maz::game {

// The (up to) two firing angles that hit a target with a FIXED launch speed: the flat "direct" shot and
// the high "mortar" arc. `reachable` is false when the target is out of range for that speed.
struct LaunchSolution {
    bool reachable = false;
    float low = 0.0f;  // shallower angle (radians)
    float high = 0.0f; // steeper (lofted) angle (radians)
};

// Solve the launch angle(s) to hit a target `range` metres away horizontally and `height` metres up (may
// be negative for a downhill target), launching at `speed` under `gravity`. `range` must be > 0. Uses
// the standard ballistic angle equation; the discriminant going negative means the target is unreachable
// at that speed.
inline LaunchSolution solveLaunchAngle(float range, float height, float speed, float gravity = 9.81f) {
    LaunchSolution s;
    if (range <= 0.0f || speed <= 0.0f || gravity <= 0.0f) {
        return s;
    }
    const float v2 = speed * speed;
    const float disc = v2 * v2 - gravity * (gravity * range * range + 2.0f * height * v2);
    if (disc < 0.0f) {
        return s; // out of range
    }
    const float root = std::sqrt(disc);
    const float gx = gravity * range;
    s.reachable = true;
    s.low = std::atan((v2 - root) / gx);
    s.high = std::atan((v2 + root) / gx);
    return s;
}

// The exact launch velocity to travel from `from` to `to` in exactly `timeOfFlight` seconds under
// gravity (pulling -Y). Always solvable for timeOfFlight > 0; pick the flight time to shape the arc
// (longer time -> higher lob). This is the "how hard and which way do I throw to land there at time T"
// primitive.
inline math::vec3 solveLaunchVelocity(const math::vec3& from, const math::vec3& to, float gravity,
                                      float timeOfFlight) {
    const float t = timeOfFlight;
    const math::vec3 d = to - from;
    return math::vec3(d.x / t, d.y / t + 0.5f * gravity * t, d.z / t);
}

// Position of a projectile launched from `from` with initial velocity `vel`, after `t` seconds under
// `gravity` (-Y). pos(0) == from.
inline math::vec3 projectilePosition(const math::vec3& from, const math::vec3& vel, float gravity,
                                     float t) {
    return math::vec3(from.x + vel.x * t, from.y + vel.y * t - 0.5f * gravity * t * t,
                      from.z + vel.z * t);
}

// Peak height reached above the launch point for a shot at `launchAngle` (radians) and `launchSpeed`.
// Meaningful only when the vertical component is upward (launchAngle in (0, pi)).
inline float projectileApex(float launchSpeed, float launchAngle, float gravity) {
    const float vy = launchSpeed * std::sin(launchAngle);
    return (vy * vy) / (2.0f * gravity);
}

// Maximum horizontal range on flat ground (target at launch height), achieved at a 45-degree launch:
// v^2 / g. Handy for a quick "is it even in range?" test before solving the angle.
inline float maxRangeFlat(float speed, float gravity = 9.81f) {
    return (gravity > 0.0f) ? (speed * speed) / gravity : 0.0f;
}

} // namespace maz::game
