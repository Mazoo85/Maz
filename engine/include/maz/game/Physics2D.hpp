#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Impulse-based 2D rigid-body dynamics — the layer above collision *detection* (this module resolves
// collisions, not just reports them). Bodies are circles or axis-aligned boxes carrying velocity, an
// inverse mass (0 = immovable/infinite mass), restitution (bounciness), and friction; the world
// integrates gravity, then resolves overlaps with a normal impulse + Coulomb friction impulse + a
// positional correction (so stacks don't sink), and bounces bodies off a static box. Deterministic
// under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly. Coordinates are whatever
// the caller uses (the demos use screen pixels with +y pointing down).

struct Body2D {
    enum Shape { Circle, Box };

    math::vec2 pos{0.0f, 0.0f};
    math::vec2 vel{0.0f, 0.0f};
    int shape = Circle;
    float radius = 0.5f;        // used when shape == Circle
    math::vec2 half{0.5f, 0.5f}; // half-extents when shape == Box
    float invMass = 1.0f;       // 0 => static (infinite mass, never moves)
    float restitution = 0.4f;   // 0 = inelastic, 1 = perfectly bouncy
    float friction = 0.0f;      // Coulomb coefficient (0 = frictionless; default keeps circle demos as-is)
};

struct Bounds2D {
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
};

// --- Contact generation: normal (pointing from a toward b) + penetration depth --------------------
namespace detail {

inline bool contactCircleCircle(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    const math::vec2 d = b.pos - a.pos;
    const float dist2 = glm::dot(d, d);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return false;
    }
    const float dist = std::sqrt(dist2);
    n = dist > 1e-6f ? d / dist : math::vec2(1.0f, 0.0f);
    pen = r - dist;
    return true;
}

inline bool contactBoxBox(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    const float dx = b.pos.x - a.pos.x;
    const float dy = b.pos.y - a.pos.y;
    const float ox = (a.half.x + b.half.x) - std::fabs(dx);
    const float oy = (a.half.y + b.half.y) - std::fabs(dy);
    if (ox <= 0.0f || oy <= 0.0f) {
        return false;
    }
    if (ox < oy) { // separate along the axis of least penetration
        n = math::vec2(dx < 0.0f ? -1.0f : 1.0f, 0.0f);
        pen = ox;
    } else {
        n = math::vec2(0.0f, dy < 0.0f ? -1.0f : 1.0f);
        pen = oy;
    }
    return true;
}

// Circle `c` vs box `x`. Returns the normal pointing from the circle toward the box + penetration.
inline bool contactCircleBox(const Body2D& c, const Body2D& x, math::vec2& nCircleToBox, float& pen) {
    const math::vec2 bmin = x.pos - x.half;
    const math::vec2 bmax = x.pos + x.half;
    const math::vec2 closest(glm::clamp(c.pos.x, bmin.x, bmax.x),
                             glm::clamp(c.pos.y, bmin.y, bmax.y));
    const math::vec2 d = c.pos - closest; // box surface -> circle center
    const float dist2 = glm::dot(d, d);
    if (dist2 > c.radius * c.radius) {
        return false;
    }
    if (dist2 > 1e-12f) { // circle center outside the box
        const float dist = std::sqrt(dist2);
        nCircleToBox = -(d / dist); // from circle toward box
        pen = c.radius - dist;
        return true;
    }
    // Circle center is inside the box: push out along the axis of least penetration.
    const float px = x.half.x - std::fabs(c.pos.x - x.pos.x);
    const float py = x.half.y - std::fabs(c.pos.y - x.pos.y);
    if (px < py) {
        nCircleToBox = math::vec2(c.pos.x < x.pos.x ? 1.0f : -1.0f, 0.0f);
        pen = px + c.radius;
    } else {
        nCircleToBox = math::vec2(0.0f, c.pos.y < x.pos.y ? 1.0f : -1.0f);
        pen = py + c.radius;
    }
    return true;
}

} // namespace detail

// Fill (n, pen) for any shape pair; n points from a toward b. Returns false if not overlapping.
inline bool contact(const Body2D& a, const Body2D& b, math::vec2& n, float& pen) {
    if (a.shape == Body2D::Circle && b.shape == Body2D::Circle) {
        return detail::contactCircleCircle(a, b, n, pen);
    }
    if (a.shape == Body2D::Box && b.shape == Body2D::Box) {
        return detail::contactBoxBox(a, b, n, pen);
    }
    if (a.shape == Body2D::Circle) { // a circle, b box
        return detail::contactCircleBox(a, b, n, pen);
    }
    // a box, b circle: compute circle->box then flip so n points a(box)->b(circle).
    math::vec2 nCircleToBox;
    if (!detail::contactCircleBox(b, a, nCircleToBox, pen)) {
        return false;
    }
    n = -nCircleToBox;
    return true;
}

// Apply a normal impulse (+ Coulomb friction) and positional correction for a known contact.
inline void resolveContact(Body2D& a, Body2D& b, const math::vec2& n, float pen) {
    const float invSum = a.invMass + b.invMass;
    if (invSum <= 0.0f) {
        return;
    }
    const math::vec2 rv = b.vel - a.vel;
    const float vn = glm::dot(rv, n);
    if (vn < 0.0f) { // only resolve if closing
        const float e = a.restitution < b.restitution ? a.restitution : b.restitution;
        const float jn = -(1.0f + e) * vn / invSum;
        const math::vec2 impulse = n * jn;
        a.vel -= impulse * a.invMass;
        b.vel += impulse * b.invMass;

        const float mu = std::sqrt(a.friction * b.friction);
        if (mu > 0.0f) {
            const math::vec2 rv2 = b.vel - a.vel;
            const math::vec2 vt = rv2 - n * glm::dot(rv2, n); // tangential relative velocity
            const float tlen = std::sqrt(glm::dot(vt, vt));
            if (tlen > 1e-6f) {
                const math::vec2 tdir = vt / tlen;
                float jt = -glm::dot(rv2, tdir) / invSum;
                const float maxF = mu * jn; // Coulomb cone
                jt = jt < -maxF ? -maxF : (jt > maxF ? maxF : jt);
                const math::vec2 fImpulse = tdir * jt;
                a.vel -= fImpulse * a.invMass;
                b.vel += fImpulse * b.invMass;
            }
        }
    }
    const float slop = 0.01f, percent = 0.8f;
    const float corrMag = (pen - slop > 0.0f ? pen - slop : 0.0f) / invSum * percent;
    const math::vec2 corr = n * corrMag;
    a.pos -= corr * a.invMass;
    b.pos += corr * b.invMass;
}

// Resolve one pair of any shapes. Returns true if they were overlapping.
inline bool collide(Body2D& a, Body2D& b) {
    math::vec2 n;
    float pen;
    if (!contact(a, b, n, pen)) {
        return false;
    }
    resolveContact(a, b, n, pen);
    return true;
}

// Backward-compatible circle-only pair resolution (unchanged numerics).
inline bool collideCircles(Body2D& a, Body2D& b) {
    math::vec2 n;
    float pen;
    if (!detail::contactCircleCircle(a, b, n, pen)) {
        return false;
    }
    resolveContact(a, b, n, pen);
    return true;
}

// Keep a body inside a static box, bouncing its velocity by its restitution on each wall it hits.
// Uses per-axis extents so boxes and circles are both handled.
inline void collideBounds(Body2D& b, const Bounds2D& bounds) {
    if (b.invMass <= 0.0f) {
        return;
    }
    const float ex = b.shape == Body2D::Box ? b.half.x : b.radius;
    const float ey = b.shape == Body2D::Box ? b.half.y : b.radius;
    const float e = b.restitution;
    if (b.pos.x - ex < bounds.minX) {
        b.pos.x = bounds.minX + ex;
        if (b.vel.x < 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.x + ex > bounds.maxX) {
        b.pos.x = bounds.maxX - ex;
        if (b.vel.x > 0.0f) b.vel.x = -b.vel.x * e;
    }
    if (b.pos.y - ey < bounds.minY) {
        b.pos.y = bounds.minY + ey;
        if (b.vel.y < 0.0f) b.vel.y = -b.vel.y * e;
    }
    if (b.pos.y + ey > bounds.maxY) {
        b.pos.y = bounds.maxY - ey;
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
                    collide(bodies[i], bodies[j]);
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
