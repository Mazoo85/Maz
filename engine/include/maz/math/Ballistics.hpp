#pragma once

#include "maz/math/Math.hpp"      // vec2, vec3
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>

// maz::math ballistics — the two aiming problems every action game needs and neither GLM nor Godot ships:
//   1. LAUNCH ANGLE: given a projectile speed and gravity, at what angle do you fire to hit a target? Under
//      gravity a reachable target has TWO solutions — a flat "direct" shot and a lobbed "mortar" arc — and
//      beyond the range limit, none. This is artillery, grenades, basketball AI, catapults, trajectory
//      previews.
//   2. INTERCEPT LEAD: where do you aim a constant-speed projectile to hit a target moving at constant
//      velocity? Solve the quadratic in time-to-impact; this is turret lead, homing-missile launch, "aim
//      ahead of the runner".
// Both are exact closed forms, verified by independently simulating the shot and confirming it lands on the
// target. The 3D helpers assume +Y is up (the engine convention) and gravity pulls along −Y. Header-only,
// std-only, deterministic.
namespace maz::math {

// Two launch angles (radians above horizontal) that hit a planar target. `count` is 0 (out of range), 1
// (target exactly at the range limit, angles equal), or 2 (a low/direct and a high/lobbed arc).
struct LaunchAngles {
    int count = 0;
    float low = 0.0f;  // the flatter, faster-arriving shot
    float high = 0.0f; // the lobbed shot
};

// Solve for the launch angle(s) to hit target (x, y) from the origin with launch `speed` under downward
// `gravity` (> 0). `x` is horizontal distance (> 0), `y` is the target's height relative to the launch point.
inline LaunchAngles ballisticAngles(float speed, float x, float y, float gravity) {
    LaunchAngles out;
    if (gravity <= 0.0f || speed <= 0.0f) {
        return out;
    }
    if (std::fabs(x) < 1e-6f) {
        // Straight-up shot: reachable iff the apex clears the target height.
        if (y <= (speed * speed) / (2.0f * gravity) + 1e-6f) {
            out.count = 1;
            out.low = out.high = 1.57079632679489662f; // 90 degrees
        }
        return out;
    }
    const float s2 = speed * speed;
    const float a = gravity * x * x / (2.0f * s2); // coefficient on tan^2(theta)
    // a*u^2 - x*u + (y + a) = 0, where u = tan(theta).
    const float disc = x * x - 4.0f * a * (y + a);
    if (disc < 0.0f) {
        return out; // target out of range
    }
    const float sq = std::sqrt(disc);
    const float uHigh = (x + sq) / (2.0f * a);
    const float uLow = (x - sq) / (2.0f * a);
    out.low = std::atan(uLow);
    out.high = std::atan(uHigh);
    out.count = (sq < 1e-6f * (std::fabs(x) + 1.0f)) ? 1 : 2;
    return out;
}

// The launch velocity vectors (low/high arc) to hit `to` from `from` at the given `speed` under downward
// gravity of magnitude `gravity`. +Y is up. `count` mirrors ballisticAngles.
struct LaunchVelocities {
    int count = 0;
    vec3 low{0.0f};
    vec3 high{0.0f};
};

inline LaunchVelocities ballisticVelocities(const vec3& from, const vec3& to, float speed, float gravity) {
    LaunchVelocities out;
    const vec3 delta = to - from;
    const vec3 horiz(delta.x, 0.0f, delta.z);
    const float hlen = std::sqrt(dot(horiz, horiz));
    const LaunchAngles ang = ballisticAngles(speed, hlen, delta.y, gravity);
    out.count = ang.count;
    if (ang.count == 0) {
        return out;
    }
    const vec3 up(0.0f, 1.0f, 0.0f);
    const vec3 hdir = hlen > 1e-6f ? horiz * (1.0f / hlen) : vec3(0.0f);
    auto vel = [&](float theta) {
        return hdir * (speed * std::cos(theta)) + up * (speed * std::sin(theta));
    };
    out.low = vel(ang.low);
    out.high = vel(ang.high);
    return out;
}

// Where to aim a projectile of constant `projectileSpeed` fired from `shooter` to hit a target at `targetPos`
// moving at constant `targetVel`. `ok` is false when the target cannot be caught. Gravity-free (straight
// shot); for a lobbed intercept, feed the aim point back into ballisticVelocities.
struct Intercept {
    bool ok = false;
    float time = 0.0f;    // time to impact (>= 0)
    vec3 aimPoint{0.0f};  // where the target will be at impact
    vec3 direction{0.0f}; // unit fire direction from the shooter
};

inline Intercept interceptLead(const vec3& shooter, float projectileSpeed, const vec3& targetPos,
                               const vec3& targetVel) {
    Intercept out;
    if (projectileSpeed <= 0.0f) {
        return out;
    }
    const vec3 d = targetPos - shooter;
    const float a = dot(targetVel, targetVel) - projectileSpeed * projectileSpeed;
    const float b = 2.0f * dot(d, targetVel);
    const float c = dot(d, d);
    float t = -1.0f;
    if (std::fabs(a) < 1e-9f) {
        // Target speed equals projectile speed: linear equation b*t + c = 0.
        if (std::fabs(b) > 1e-12f) {
            const float cand = -c / b;
            if (cand > 0.0f) {
                t = cand;
            }
        }
    } else {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float sq = std::sqrt(disc);
            const float t0 = (-b - sq) / (2.0f * a);
            const float t1 = (-b + sq) / (2.0f * a);
            // Smallest strictly positive root.
            if (t0 > 1e-6f && (t1 <= 1e-6f || t0 < t1)) {
                t = t0;
            } else if (t1 > 1e-6f) {
                t = t1;
            }
        }
    }
    if (t <= 0.0f) {
        return out;
    }
    out.ok = true;
    out.time = t;
    out.aimPoint = targetPos + targetVel * t;
    const vec3 dir = out.aimPoint - shooter;
    const float dl = std::sqrt(dot(dir, dir));
    out.direction = dl > 1e-9f ? dir * (1.0f / dl) : vec3(0.0f);
    return out;
}

} // namespace maz::math
