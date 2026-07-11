#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Reynolds-style steering behaviors for autonomous agents. Each behavior returns a steering *force*
// (an acceleration request, already clamped to the agent's maxForce); the caller sums the forces it
// wants, then calls integrate() to apply them. Forces compose linearly, so seek + separation +
// path-following just add together. Pure vector math — no GPU/allocation, so it unit-tests headless.
//
// Convention: full 3D vectors, but games that move on the ground can simply keep y fixed (the demo
// zeroes the y component of every force). Speeds/forces are in world-units/second and /second^2.

struct Agent {
    math::vec3 pos{0.0f};
    math::vec3 vel{0.0f};
    float maxSpeed = 6.0f;  // velocity magnitude ceiling
    float maxForce = 24.0f; // steering acceleration ceiling
};

// Clamp a vector to a maximum magnitude (leaves shorter vectors untouched; zero stays zero).
inline math::vec3 limit(const math::vec3& v, float maxLen) {
    const float len2 = math::dot(v, v);
    if (maxLen <= 0.0f || len2 <= maxLen * maxLen || len2 <= 1e-12f) {
        return v;
    }
    return v * (maxLen / std::sqrt(len2));
}

// Steer toward `target` at full speed: force = (desiredVelocity - currentVelocity), clamped.
inline math::vec3 seek(const Agent& a, const math::vec3& target) {
    const math::vec3 offset = target - a.pos;
    const float d2 = math::dot(offset, offset);
    if (d2 <= 1e-12f) {
        return math::vec3(0.0f);
    }
    const math::vec3 desired = offset * (a.maxSpeed / std::sqrt(d2));
    return limit(desired - a.vel, a.maxForce);
}

// Steer directly away from `target` (opposite of seek).
inline math::vec3 flee(const Agent& a, const math::vec3& target) {
    const math::vec3 offset = a.pos - target;
    const float d2 = math::dot(offset, offset);
    if (d2 <= 1e-12f) {
        return math::vec3(0.0f);
    }
    const math::vec3 desired = offset * (a.maxSpeed / std::sqrt(d2));
    return limit(desired - a.vel, a.maxForce);
}

// Seek `target` but ramp speed down inside `slowRadius` so the agent eases to a stop on it instead
// of overshooting/orbiting. Outside the radius it behaves like seek.
inline math::vec3 arrive(const Agent& a, const math::vec3& target, float slowRadius) {
    const math::vec3 offset = target - a.pos;
    const float dist = std::sqrt(math::dot(offset, offset));
    if (dist <= 1e-5f) {
        return limit(-a.vel, a.maxForce); // on target: brake
    }
    float speed = a.maxSpeed;
    if (slowRadius > 0.0f && dist < slowRadius) {
        speed = a.maxSpeed * (dist / slowRadius);
    }
    const math::vec3 desired = offset * (speed / dist);
    return limit(desired - a.vel, a.maxForce);
}

// Push away from neighbors within `radius`, weighted by 1/distance so closer crowding pushes harder.
// `neighborPositions` may include the agent's own position; a zero-distance self-entry is skipped.
inline math::vec3 separation(const Agent& a, const std::vector<math::vec3>& neighborPositions,
                             float radius) {
    if (radius <= 0.0f) {
        return math::vec3(0.0f);
    }
    math::vec3 push(0.0f);
    int count = 0;
    for (const math::vec3& n : neighborPositions) {
        const math::vec3 away = a.pos - n;
        const float d2 = math::dot(away, away);
        if (d2 <= 1e-8f || d2 >= radius * radius) {
            continue; // self, or out of range
        }
        push += away * (1.0f / d2); // inverse-square weighting, direction preserved
        ++count;
    }
    if (count == 0) {
        return math::vec3(0.0f);
    }
    // Turn the accumulated push into a desired velocity at full speed, then into a steering force.
    const float len2 = math::dot(push, push);
    if (len2 <= 1e-12f) {
        return math::vec3(0.0f);
    }
    const math::vec3 desired = push * (a.maxSpeed / std::sqrt(len2));
    return limit(desired - a.vel, a.maxForce);
}

// Advance the agent by one step: force is applied as acceleration, velocity is capped at maxSpeed,
// position integrates the new velocity. Semi-implicit Euler (update vel before pos) for stability.
inline void integrate(Agent& a, const math::vec3& force, float dt) {
    a.vel = limit(a.vel + limit(force, a.maxForce) * dt, a.maxSpeed);
    a.pos += a.vel * dt;
}

// Follow a polyline of `waypoints`, arriving at each in turn. `index` (in/out) is the current target
// waypoint; when the agent gets within `advanceRadius` of it, `index` advances. `slowRadius` eases
// the arrival at the final waypoint. Returns the steering force (zero once the path is exhausted).
inline math::vec3 followPath(const Agent& a, const std::vector<math::vec3>& waypoints,
                             uint32_t& index, float advanceRadius, float slowRadius) {
    if (waypoints.empty() || index >= waypoints.size()) {
        return math::vec3(0.0f);
    }
    const math::vec3 target = waypoints[index];
    const math::vec3 offset = target - a.pos;
    const bool last = index + 1 >= waypoints.size();
    if (math::dot(offset, offset) <= advanceRadius * advanceRadius && !last) {
        ++index; // reached this waypoint; head to the next one next step
        return seek(a, waypoints[index]);
    }
    return last ? arrive(a, target, slowRadius) : seek(a, target);
}

} // namespace maz::game
