#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <vector>

namespace maz::game {

// Local collision avoidance — Godot's NavigationAgent2D avoidance (RVO). Each agent has a PREFERRED
// velocity (usually "toward my goal"); rvoVelocity nudges it to a nearby velocity that won't run into
// moving neighbours, by sampling candidate velocities and scoring each on time-to-collision plus how
// far it strays from the preference. It is RECIPROCAL: every agent runs the same rule and each takes
// half the avoidance (the velocity-obstacle is centred on the average velocity, `2·c − vA − vB`), so a
// pair on a head-on course peels apart smoothly instead of oscillating. Pure 2D math — deterministic,
// no GPU — so it unit-tests headlessly and a scene replays identically.

struct AvoidNeighbor {
    math::vec2 pos{0.0f, 0.0f};
    math::vec2 vel{0.0f, 0.0f};
    float radius = 0.5f;
};

namespace detail {

// Earliest time t >= 0 at which a disc of radius `r` at `relPos`, closing at relative velocity `relVel`
// (its position is relPos − relVel·t), touches the origin. 0 if already overlapping, −1 if it never will.
inline float timeToCollision(math::vec2 relPos, math::vec2 relVel, float r) {
    const float c = glm::dot(relPos, relPos) - r * r;
    if (c < 0.0f) {
        return 0.0f; // already overlapping
    }
    const float a = glm::dot(relVel, relVel);
    if (a < 1e-8f) {
        return -1.0f; // no relative motion -> never collide (not already overlapping)
    }
    const float b = glm::dot(relPos, relVel);
    const float disc = b * b - a * c;
    if (disc < 0.0f) {
        return -1.0f; // paths miss
    }
    const float t = (b - std::sqrt(disc)) / a;
    return t >= 0.0f ? t : -1.0f; // negative -> collision is in the past / moving apart
}

} // namespace detail

// Choose a collision-avoiding velocity near `prefVel` for an agent at `pos` currently moving at `vel`.
// `timeHorizon` is how far ahead collisions are considered; larger = more cautious. Candidate sampling
// always includes the preferred velocity and zero, so with no threat the preferred velocity wins exactly.
inline math::vec2 rvoVelocity(math::vec2 pos, math::vec2 vel, math::vec2 prefVel, float radius,
                              float maxSpeed, const std::vector<AvoidNeighbor>& neighbors,
                              float timeHorizon = 2.0f) {
    // Build candidate velocities: the preference, a full stop, and a fan of directions × speeds.
    std::vector<math::vec2> candidates;
    candidates.reserve(70);
    candidates.push_back(prefVel);
    candidates.push_back(math::vec2(0.0f, 0.0f));
    const int dirs = 16, speeds = 4;
    for (int si = 1; si <= speeds; ++si) {
        const float sp = maxSpeed * static_cast<float>(si) / static_cast<float>(speeds);
        for (int di = 0; di < dirs; ++di) {
            const float ang = 6.2831853f * static_cast<float>(di) / static_cast<float>(dirs);
            candidates.push_back(math::vec2(std::cos(ang) * sp, std::sin(ang) * sp));
        }
    }

    const float avoidWeight = maxSpeed; // trades avoidance urgency against staying on-preference
    math::vec2 best = prefVel;
    float bestCost = 1e30f;
    for (const math::vec2& c : candidates) {
        // Penalty for the most imminent collision this candidate would cause (reciprocal VO).
        float worst = 0.0f;
        for (const AvoidNeighbor& nb : neighbors) {
            const math::vec2 relPos = nb.pos - pos;
            const math::vec2 u = c * 2.0f - vel - nb.vel; // reciprocal relative velocity
            const float tc = detail::timeToCollision(relPos, u, radius + nb.radius);
            if (tc == 0.0f) {
                worst = 1e6f; // colliding right now: strongly discouraged
                break;
            }
            if (tc > 0.0f && tc < timeHorizon) {
                const float pen = 1.0f / tc;
                if (pen > worst) {
                    worst = pen;
                }
            }
        }
        const math::vec2 dv = c - prefVel;
        const float cost = std::sqrt(glm::dot(dv, dv)) + avoidWeight * worst;
        if (cost < bestCost) {
            bestCost = cost;
            best = c;
        }
    }
    return best;
}

} // namespace maz::game
