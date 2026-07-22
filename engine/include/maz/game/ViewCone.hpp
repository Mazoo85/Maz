#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

// maz::game VIEW CONE — the continuous "can this AI see that?" perception test: is a target within an observer's
// sight RANGE and inside its field-of-view CONE (half-angle around a facing direction)? This is the geometry every
// stealth guard, turret, sentry, or aggro check needs — distinct from the grid-based shadowcasting `FieldOfView`
// (which lights tiles). Pure angle+distance math; combine with a line-of-sight/raycast check for wall occlusion
// (kept separate so you choose the occluder source). Works in 2D and 3D. Header-only, deterministic. Godot has no
// built-in vision-cone helper, so this is a beyond-Godot gameplay utility.
namespace maz::game {

// Detail of a 2D view-cone query.
struct ViewSample {
    bool inCone = false; // within both range and the cone angle
    float distance = 0.0f;
    float angle = 0.0f; // radians between the facing direction and the target
};

// 2D: full query — distance to target, angular offset from `facing`, and whether it's within `maxRange` and
// `halfAngle`. `facing` need not be normalised. A target on top of the observer counts as visible.
inline ViewSample sampleViewCone2D(const math::vec2& observer, const math::vec2& facing, const math::vec2& target,
                                   float halfAngle, float maxRange) {
    ViewSample s;
    const math::vec2 to = target - observer;
    s.distance = std::sqrt(to.x * to.x + to.y * to.y);
    if (s.distance <= 1e-6f) { s.inCone = s.distance <= maxRange; s.angle = 0.0f; return s; }
    const float fl = std::sqrt(facing.x * facing.x + facing.y * facing.y);
    if (fl <= 1e-6f) return s; // no facing -> sees nothing
    float c = (facing.x * to.x + facing.y * to.y) / (fl * s.distance);
    c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    s.angle = std::acos(c);
    s.inCone = s.distance <= maxRange && s.angle <= halfAngle;
    return s;
}

// 2D: is `target` inside the observer's view cone (within `maxRange` and `halfAngle` of `facing`)?
inline bool inViewCone2D(const math::vec2& observer, const math::vec2& facing, const math::vec2& target,
                         float halfAngle, float maxRange) {
    return sampleViewCone2D(observer, facing, target, halfAngle, maxRange).inCone;
}

// 3D: is `target` inside the observer's view cone around `facing` (within `maxRange` and `halfAngle`)?
inline bool inViewCone3D(const math::vec3& observer, const math::vec3& facing, const math::vec3& target,
                         float halfAngle, float maxRange) {
    const math::vec3 to = target - observer;
    const float dist = std::sqrt(math::dot(to, to));
    if (dist <= 1e-6f) return dist <= maxRange;
    if (dist > maxRange) return false;
    const float fl = std::sqrt(math::dot(facing, facing));
    if (fl <= 1e-6f) return false;
    float c = math::dot(facing, to) / (fl * dist);
    c = c < -1.0f ? -1.0f : (c > 1.0f ? 1.0f : c);
    return std::acos(c) <= halfAngle;
}

} // namespace maz::game
