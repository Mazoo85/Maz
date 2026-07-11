#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Impulse-based 2D rigid-body dynamics for circles — the layer above collision *detection* (this
// module resolves collisions, not just reports them). Bodies carry velocity, an inverse mass
// (0 = immovable/infinite mass), and restitution (bounciness); the world integrates gravity, then
// resolves circle-circle overlaps with a normal impulse plus positional correction (so stacks don't
// sink), and bounces bodies off a static box. Deterministic under a fixed timestep and free of GPU/
// RNG, so it unit-tests headlessly. Coordinates are whatever the caller uses (the demo uses screen
// pixels with +y pointing down).

struct Body2D {
    math::vec2 pos{0.0f, 0.0f};
    math::vec2 vel{0.0f, 0.0f};
    float radius = 0.5f;
    float invMass = 1.0f;      // 0 => static (infinite mass, never moves)
    float restitution = 0.4f;  // 0 = inelastic, 1 = perfectly bouncy
};

struct Bounds2D {
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
};

// Resolve one circle-circle pair in place: applies a normal impulse (if the bodies are approaching)
// and positional correction to remove overlap. Returns true if the pair was overlapping.
inline bool collideCircles(Body2D& a, Body2D& b) {
    const math::vec2 delta = b.pos - a.pos;
    const float dist2 = glm::dot(delta, delta);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return false;
    }
    const float invSum = a.invMass + b.invMass;
    if (invSum <= 0.0f) {
        return true; // two static bodies: nothing to move
    }
    const float dist = std::sqrt(dist2);
    // Degenerate coincident centers: pick an arbitrary normal so they still separate.
    const math::vec2 n = dist > 1e-6f ? delta / dist : math::vec2(1.0f, 0.0f);
    const float penetration = r - dist;

    const math::vec2 rv = b.vel - a.vel;
    const float vn = glm::dot(rv, n);
    if (vn < 0.0f) { // only resolve if they're closing
        const float e = a.restitution < b.restitution ? a.restitution : b.restitution;
        const float j = -(1.0f + e) * vn / invSum;
        const math::vec2 impulse = n * j;
        a.vel -= impulse * a.invMass;
        b.vel += impulse * b.invMass;
    }
    // Baumgarte positional correction with a small slop so resting stacks don't jitter.
    const float slop = 0.01f, percent = 0.8f;
    const float corrMag = (penetration - slop > 0.0f ? penetration - slop : 0.0f) / invSum * percent;
    const math::vec2 corr = n * corrMag;
    a.pos -= corr * a.invMass;
    b.pos += corr * b.invMass;
    return true;
}

// Keep a body inside a static box, bouncing its velocity by its restitution on each wall it hits.
inline void collideBounds(Body2D& b, const Bounds2D& bounds) {
    if (b.invMass <= 0.0f) {
        return;
    }
    const float e = b.restitution;
    if (b.pos.x - b.radius < bounds.minX) {
        b.pos.x = bounds.minX + b.radius;
        if (b.vel.x < 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.x + b.radius > bounds.maxX) {
        b.pos.x = bounds.maxX - b.radius;
        if (b.vel.x > 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.y - b.radius < bounds.minY) {
        b.pos.y = bounds.minY + b.radius;
        if (b.vel.y < 0.0f) b.vel.y = -b.vel.y * e;
    }
    if (b.pos.y + b.radius > bounds.maxY) {
        b.pos.y = bounds.maxY - b.radius;
        if (b.vel.y > 0.0f) b.vel.y = -b.vel.y * e;
    }
}

class PhysicsWorld2D {
public:
    math::vec2 gravity{0.0f, 0.0f};
    Bounds2D bounds{};
    bool hasBounds = false;
    std::vector<Body2D> bodies;

    uint32_t add(const Body2D& b) {
        bodies.push_back(b);
        return static_cast<uint32_t>(bodies.size() - 1);
    }

    // Advance the simulation by dt: integrate gravity + motion, then resolve contacts for `iterations`
    // passes (more iterations = stiffer stacks). Body-body pairs are resolved before the walls.
    void step(float dt, int iterations = 4) {
        for (Body2D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
            }
        }
        for (Body2D& b : bodies) {
            b.pos += b.vel * dt;
        }
        for (int it = 0; it < iterations; ++it) {
            for (size_t i = 0; i < bodies.size(); ++i) {
                for (size_t j = i + 1; j < bodies.size(); ++j) {
                    collideCircles(bodies[i], bodies[j]);
                }
            }
            if (hasBounds) {
                for (Body2D& b : bodies) {
                    collideBounds(b, bounds);
                }
            }
        }
    }
};

} // namespace maz::game
