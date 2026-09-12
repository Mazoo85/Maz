#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

// maz::math projectile lead / intercept solver — "aim ahead of a moving target." Given a shooter, a
// target's current position and velocity, and how fast the projectile travels, find WHERE to aim so a
// shot fired now meets the target: the time-to-intercept, the future point to aim at, and the unit aim
// direction. This is the math behind every turret, homing shot, AI marksman, and "lead the duck"
// mechanic. It's a quadratic in the intercept time t — solve |targetPos + targetVel·t − shooter| =
// projSpeed·t — which can have zero solutions (the target outruns the projectile), one, or two (pick
// the soonest). Pure closed-form math over vec2/vec3, no allocation, so it unit-tests exactly.
namespace maz::math {

// Smallest strictly-positive root of a·t² + b·t + c = 0, or -1 if there is none. Handles the
// degenerate a≈0 (linear) case — which occurs when the target's speed equals the projectile's.
inline float smallestPositiveRoot(float a, float b, float c) {
    if (std::fabs(a) < 1e-6f) {
        if (std::fabs(b) < 1e-9f) {
            return -1.0f;
        }
        const float t = -c / b;
        return t > 0.0f ? t : -1.0f;
    }
    const float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) {
        return -1.0f;
    }
    const float sq = std::sqrt(disc);
    const float t1 = (-b - sq) / (2.0f * a);
    const float t2 = (-b + sq) / (2.0f * a);
    float best = -1.0f;
    if (t1 > 0.0f) {
        best = t1;
    }
    if (t2 > 0.0f && (best < 0.0f || t2 < best)) {
        best = t2;
    }
    return best;
}

struct Intercept2D {
    bool hit = false;
    float time = 0.0f;             // seconds until the projectile meets the target
    vec2 aimPoint{0.0f, 0.0f};     // the future target position — aim here
    vec2 direction{0.0f, 0.0f};    // unit aim direction from the shooter (zero if degenerate)
};

// 2D intercept. projSpeed is the projectile's constant speed. Returns hit=false when the target cannot
// be caught (e.g. it moves away at or above the projectile's speed).
inline Intercept2D interceptTarget(vec2 shooter, vec2 targetPos, vec2 targetVel, float projSpeed) {
    Intercept2D r;
    const vec2 d = targetPos - shooter;
    const float a = (targetVel.x * targetVel.x + targetVel.y * targetVel.y) - projSpeed * projSpeed;
    const float b = 2.0f * (d.x * targetVel.x + d.y * targetVel.y);
    const float c = d.x * d.x + d.y * d.y;
    const float t = smallestPositiveRoot(a, b, c);
    if (t < 0.0f) {
        return r;
    }
    r.hit = true;
    r.time = t;
    r.aimPoint = targetPos + targetVel * t;
    const vec2 aim = r.aimPoint - shooter;
    const float len = std::sqrt(aim.x * aim.x + aim.y * aim.y);
    if (len > 0.0f) {
        r.direction = vec2(aim.x / len, aim.y / len);
    }
    return r;
}

struct Intercept3D {
    bool hit = false;
    float time = 0.0f;
    vec3 aimPoint{0.0f, 0.0f, 0.0f};
    vec3 direction{0.0f, 0.0f, 0.0f};
};

// 3D intercept (same closed form, one more component).
inline Intercept3D interceptTarget(vec3 shooter, vec3 targetPos, vec3 targetVel, float projSpeed) {
    Intercept3D r;
    const vec3 d = targetPos - shooter;
    const float a =
        (targetVel.x * targetVel.x + targetVel.y * targetVel.y + targetVel.z * targetVel.z) -
        projSpeed * projSpeed;
    const float b = 2.0f * (d.x * targetVel.x + d.y * targetVel.y + d.z * targetVel.z);
    const float c = d.x * d.x + d.y * d.y + d.z * d.z;
    const float t = smallestPositiveRoot(a, b, c);
    if (t < 0.0f) {
        return r;
    }
    r.hit = true;
    r.time = t;
    r.aimPoint = targetPos + targetVel * t;
    const vec3 aim = r.aimPoint - shooter;
    const float len = std::sqrt(aim.x * aim.x + aim.y * aim.y + aim.z * aim.z);
    if (len > 0.0f) {
        r.direction = vec3(aim.x / len, aim.y / len, aim.z / len);
    }
    return r;
}

} // namespace maz::math
