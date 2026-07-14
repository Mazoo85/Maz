#pragma once

#include "maz/game/CollisionLayers.hpp"
#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::game {

// Impulse-based 3D rigid-body dynamics — the 3D sibling of Physics2D. This module resolves collisions
// (it doesn't just report them). Bodies carry a position, linear velocity, an inverse mass (0 =
// immovable/infinite mass), restitution (bounciness) and friction; the world integrates gravity with
// semi-implicit Euler, detects contacts, and resolves them with a normal impulse + Coulomb friction
// impulse + a positional correction (so resting bodies don't sink). Deterministic under a fixed
// timestep and free of GPU/RNG, so it unit-tests headlessly.
//
// D1 (this milestone) ships the foundation: dynamic Spheres, an infinite static ground Plane
// (half-space), gravity, and sphere-sphere + sphere-plane resolution. Orientation/spin/inertia fields
// are present but locked (invInertia zero) so later milestones (boxes, angular dynamics, manifolds,
// warm-started stacking, broadphase, sleeping, queries, character controller) drop in without churn —
// the same way Physics2D grew. Coordinates are whatever the caller uses; the demos use a Y-up world.

struct Body3D {
    // A Sphere is a point + `radius`. A Plane is an infinite static half-space (Godot
    // WorldBoundaryShape3D): `normal` is the unit outward normal (toward the free space where bodies
    // live) and `planeD` the plane offset, so the solid region is { p : dot(p, normal) < planeD };
    // build one with makeGroundPlane(). Box/Capsule shapes arrive in later milestones.
    enum Shape { Sphere, Plane };

    math::vec3 pos{0.0f, 0.0f, 0.0f};
    math::vec3 vel{0.0f, 0.0f, 0.0f};
    int shape = Sphere;
    float radius = 0.5f;                 // used when shape == Sphere
    math::vec3 normal{0.0f, 1.0f, 0.0f}; // unit outward normal when shape == Plane
    float planeD = 0.0f;                 // plane offset when shape == Plane

    float invMass = 1.0f;     // 0 => static (infinite mass, never moves)
    float restitution = 0.3f; // 0 = inelastic, 1 = perfectly bouncy
    float friction = 0.5f;    // Coulomb coefficient (0 = frictionless)
    float linearDamping = 0.0f;  // per-second velocity decay (0 => none), like Godot linear_damp

    // --- Rotation (locked in D1, wired up in a later milestone) -------------------------------------
    // A body carries an orientation and spin, but rotation is locked by default: invInertia is the zero
    // matrix (infinite moment of inertia), so contact impulses produce no torque. Angular dynamics land
    // in a later milestone; carrying the fields now keeps that a purely additive change.
    math::quat orientation{1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    math::vec3 angularVel{0.0f, 0.0f, 0.0f};
    math::mat3 invInertia{0.0f}; // zero => rotation locked

    // --- Collision filtering (Godot collision_layer / collision_mask) ------------------------------
    LayerMask collisionLayer = ~0u;
    LayerMask collisionMask = ~0u;
};

// Build a dynamic sphere body at `pos` with the given radius and mass (mass <= 0 => static).
inline Body3D makeSphere(math::vec3 pos, float radius, float mass = 1.0f) {
    Body3D b;
    b.shape = Body3D::Sphere;
    b.pos = pos;
    b.radius = radius;
    b.invMass = mass > 0.0f ? 1.0f / mass : 0.0f;
    return b;
}

// Build a static infinite ground plane (half-space) from an outward unit normal and any point on it —
// Godot WorldBoundaryShape3D. Bodies live on the +normal side; the solid fills the -normal side.
inline Body3D makeGroundPlane(math::vec3 normal, math::vec3 pointOnPlane) {
    Body3D b;
    b.shape = Body3D::Plane;
    const float len = std::sqrt(glm::dot(normal, normal));
    b.normal = len > 1e-9f ? normal / len : math::vec3(0.0f, 1.0f, 0.0f);
    b.planeD = glm::dot(b.normal, pointOnPlane);
    b.friction = 0.6f; // grippy ground so bodies come to rest instead of sliding forever
    b.invMass = 0.0f;
    return b;
}

namespace detail {

// A single contact: `n` points from body a toward body b, `pen` is the overlap depth (>0), `point` is
// a representative world contact point.
struct Contact3 {
    int a = -1, b = -1;
    math::vec3 n{0.0f, 1.0f, 0.0f};
    math::vec3 point{0.0f, 0.0f, 0.0f};
    float pen = 0.0f;
    bool hit = false;
};

// Sphere a vs sphere b. n from a -> b.
inline Contact3 sphereSphere(int ia, const Body3D& a, int ib, const Body3D& b) {
    Contact3 c;
    const math::vec3 d = b.pos - a.pos;
    const float dist2 = glm::dot(d, d);
    const float r = a.radius + b.radius;
    if (dist2 >= r * r) {
        return c;
    }
    const float dist = std::sqrt(dist2);
    const math::vec3 n = dist > 1e-6f ? d / dist : math::vec3(0.0f, 1.0f, 0.0f);
    c.a = ia;
    c.b = ib;
    c.n = n;
    c.pen = r - dist;
    c.point = a.pos + n * a.radius;
    c.hit = true;
    return c;
}

// Sphere `s` vs static plane `p`. n from s -> plane (into the solid, i.e. -plane.normal).
inline Contact3 spherePlane(int is, const Body3D& s, int ip, const Body3D& p) {
    Contact3 c;
    const float sd = glm::dot(s.pos, p.normal) - p.planeD; // signed distance, +normal side positive
    const float pen = s.radius - sd;
    if (pen <= 0.0f) {
        return c;
    }
    c.a = is;
    c.b = ip;
    c.n = -p.normal; // sphere -> solid
    c.pen = pen;
    c.point = s.pos - p.normal * s.radius; // deepest point on the sphere
    c.hit = true;
    return c;
}

} // namespace detail

// A world of 3D rigid bodies resolved with sequential impulses under a fixed timestep.
struct PhysicsWorld3D {
    std::vector<Body3D> bodies;
    math::vec3 gravity{0.0f, -9.81f, 0.0f};

    // Positional correction (Baumgarte): push overlapping bodies apart by `correctionPercent` of the
    // penetration beyond `slop` each step, so resting stacks don't sink but don't jitter either.
    float slop = 0.005f;
    float correctionPercent = 0.2f;
    // Relative normal speed below which restitution is suppressed, so resting bodies don't buzz.
    float restitutionThreshold = 1.0f;

    int add(const Body3D& b) {
        bodies.push_back(b);
        return static_cast<int>(bodies.size()) - 1;
    }

    void step(float dt, int iterations = 8) {
        // Integrate velocity (gravity + damping) for dynamic bodies.
        for (Body3D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.vel += gravity * dt;
                b.vel *= 1.0f / (1.0f + b.linearDamping * dt);
            }
        }

        // Detect contacts (brute-force pairs in D1; broadphase arrives in a later milestone).
        std::vector<detail::Contact3> contacts;
        const int n = static_cast<int>(bodies.size());
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (bodies[i].invMass == 0.0f && bodies[j].invMass == 0.0f) {
                    continue; // two static bodies never interact
                }
                if (!interact(bodies[i].collisionLayer, bodies[i].collisionMask,
                              bodies[j].collisionLayer, bodies[j].collisionMask)) {
                    continue;
                }
                contacts.push_back(narrow(i, j));
            }
        }

        // Velocity resolution: several sequential-impulse passes over all contacts.
        for (int it = 0; it < iterations; ++it) {
            for (const detail::Contact3& c : contacts) {
                if (c.hit) {
                    resolveVelocity(c);
                }
            }
        }
        // Positional correction (split from velocity so it never injects energy).
        for (const detail::Contact3& c : contacts) {
            if (c.hit) {
                correctPosition(c);
            }
        }

        // Integrate position.
        for (Body3D& b : bodies) {
            if (b.invMass > 0.0f) {
                b.pos += b.vel * dt;
            }
        }
    }

private:
    detail::Contact3 narrow(int i, int j) const {
        const Body3D& a = bodies[static_cast<size_t>(i)];
        const Body3D& b = bodies[static_cast<size_t>(j)];
        if (a.shape == Body3D::Sphere && b.shape == Body3D::Sphere) {
            return detail::sphereSphere(i, a, j, b);
        }
        if (a.shape == Body3D::Sphere && b.shape == Body3D::Plane) {
            return detail::spherePlane(i, a, j, b);
        }
        if (a.shape == Body3D::Plane && b.shape == Body3D::Sphere) {
            detail::Contact3 c = detail::spherePlane(j, b, i, a); // n: sphere(b) -> plane(a)
            std::swap(c.a, c.b);
            c.n = -c.n; // re-orient a -> b
            return c;
        }
        return detail::Contact3{};
    }

    void resolveVelocity(const detail::Contact3& c) {
        Body3D& a = bodies[static_cast<size_t>(c.a)];
        Body3D& b = bodies[static_cast<size_t>(c.b)];
        const float invSum = a.invMass + b.invMass;
        if (invSum <= 0.0f) {
            return;
        }
        const math::vec3 rv = b.vel - a.vel; // relative velocity of b w.r.t a
        const float vn = glm::dot(rv, c.n);
        if (vn > 0.0f) {
            return; // separating already
        }
        const float e = (-vn > restitutionThreshold) ? std::min(a.restitution, b.restitution) : 0.0f;
        const float jn = -(1.0f + e) * vn / invSum;
        const math::vec3 impulse = c.n * jn;
        a.vel -= impulse * a.invMass;
        b.vel += impulse * b.invMass;

        // Coulomb friction along the tangent of the (post-normal) relative velocity.
        const math::vec3 rv2 = b.vel - a.vel;
        math::vec3 t = rv2 - c.n * glm::dot(rv2, c.n);
        const float tl = std::sqrt(glm::dot(t, t));
        if (tl > 1e-6f) {
            t /= tl;
            const float jt = -glm::dot(rv2, t) / invSum;
            const float mu = std::sqrt(a.friction * b.friction);
            const float jtClamped = std::clamp(jt, -jn * mu, jn * mu);
            const math::vec3 fImpulse = t * jtClamped;
            a.vel -= fImpulse * a.invMass;
            b.vel += fImpulse * b.invMass;
        }
    }

    void correctPosition(const detail::Contact3& c) {
        Body3D& a = bodies[static_cast<size_t>(c.a)];
        Body3D& b = bodies[static_cast<size_t>(c.b)];
        const float invSum = a.invMass + b.invMass;
        if (invSum <= 0.0f) {
            return;
        }
        const float mag = std::max(c.pen - slop, 0.0f) * correctionPercent / invSum;
        const math::vec3 correction = c.n * mag;
        a.pos -= correction * a.invMass;
        b.pos += correction * b.invMass;
    }
};

} // namespace maz::game
