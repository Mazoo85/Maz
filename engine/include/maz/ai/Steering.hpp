#pragma once

// Classic Reynolds steering behaviors — seek/flee/arrive/pursue/evade — each
// returning a steering FORCE (an acceleration, truncated to the Agent's maxForce)
// to add to the agent's velocity each frame. seek/flee drive toward/away from a
// point; arrive ramps speed down within slowRadius so the agent decelerates to a
// smooth stop at the target; pursue/evade seek/flee the target's linearly-predicted
// future position (targetPos + targetVel*lookahead). safeNormalize/truncate guard
// zero-length vectors so no NaN escapes (glm::normalize on a zero vector yields
// NaN — never call it directly here). All behaviors operate in 3D vec3 (2D is just
// z=0). The Godot AI-steering analog; composes iter2-era maz::math (glm vec3).
// These are pure functions — stateless, not tied to any Agent instance beyond the
// const& argument, and therefore reentrant / thread-safe. Obstacle avoidance, path
// following, wander, and flocking (separation/cohesion/alignment) are future
// refinements (not built here).

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

namespace maz::ai {

using maz::math::vec3;

// A mobile agent: current kinematic state plus its steering limits.
struct Agent {
    vec3 position{0.0f};
    vec3 velocity{0.0f};
    float maxSpeed = 1.0f;  // max desired speed magnitude
    float maxForce = 1.0f;  // max steering-force magnitude (how fast it can change velocity)
};

// Small tolerance for treating a vector as zero-length (compared squared to avoid a sqrt).
constexpr float kEpsilon = 1e-6f;
constexpr float kEpsilon2 = kEpsilon * kEpsilon;

// Squared length (avoids a sqrt when only a comparison is needed).
inline float lengthSq(vec3 v) { return glm::dot(v, v); }

// Unit vector in the direction of v, or zero if v is (near) zero-length. Guards
// the divide-by-zero / NaN that glm::normalize would produce on a zero vector.
inline vec3 safeNormalize(vec3 v) {
    if (lengthSq(v) <= kEpsilon2) { return vec3{0.0f}; }
    return v / glm::length(v);
}

// Caps a vector's magnitude at maxLen, scaling DOWN only (a shorter vector is
// returned unchanged). maxLen must be non-negative.
inline vec3 truncate(vec3 v, float maxLen) {
    MAZ_ASSERT(maxLen >= 0.0f, "Steering truncate: maxLen must be >= 0");
    if (lengthSq(v) <= maxLen * maxLen) { return v; }
    return v * (maxLen / glm::length(v));
}

// Steer toward a point: desired velocity is maxSpeed straight at the target, the
// steering force is (desired - velocity) capped to maxForce. If already at the
// target, desired is zero so the force is -velocity (a braking force), truncated.
inline vec3 seek(const Agent& a, vec3 target) {
    vec3 desired = safeNormalize(target - a.position) * a.maxSpeed;
    vec3 steering = desired - a.velocity;
    return truncate(steering, a.maxForce);
}

// Steer away from a point: like seek but the desired velocity points AWAY from the
// target (position - target).
inline vec3 flee(const Agent& a, vec3 target) {
    vec3 desired = safeNormalize(a.position - target) * a.maxSpeed;
    vec3 steering = desired - a.velocity;
    return truncate(steering, a.maxForce);
}

// Seek that ramps the desired speed down linearly inside slowRadius so the agent
// decelerates to a stop AT the target. Outside slowRadius it behaves like seek.
inline vec3 arrive(const Agent& a, vec3 target, float slowRadius) {
    MAZ_ASSERT(slowRadius >= 0.0f, "Steering arrive: slowRadius must be >= 0");
    vec3 offset = target - a.position;
    float dist = glm::length(offset);
    if (dist <= kEpsilon) {
        // At the target: desired speed 0, so brake.
        return truncate(-a.velocity, a.maxForce);
    }
    float speed = a.maxSpeed;
    if (slowRadius > 0.0f && dist < slowRadius) {
        speed = a.maxSpeed * (dist / slowRadius);
    }
    vec3 desired = (offset / dist) * speed;
    vec3 steering = desired - a.velocity;
    return truncate(steering, a.maxForce);
}

// Seek the target's linearly-predicted future position (targetPos + targetVel*lookahead).
inline vec3 pursue(const Agent& a, vec3 targetPos, vec3 targetVel, float lookahead) {
    MAZ_ASSERT(lookahead >= 0.0f, "Steering pursue: lookahead must be >= 0");
    vec3 future = targetPos + targetVel * lookahead;
    return seek(a, future);
}

// Flee the target's linearly-predicted future position (targetPos + targetVel*lookahead).
inline vec3 evade(const Agent& a, vec3 targetPos, vec3 targetVel, float lookahead) {
    MAZ_ASSERT(lookahead >= 0.0f, "Steering evade: lookahead must be >= 0");
    vec3 future = targetPos + targetVel * lookahead;
    return flee(a, future);
}

} // namespace maz::ai
