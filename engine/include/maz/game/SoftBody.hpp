#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game soft bodies — Godot's SoftBody3D (cloth, rope, jelly). Rather than a rigid transform, a soft
// body is a cloud of point masses linked by distance constraints, simulated with Position-Based
// Dynamics: each step Verlet-integrates the particles, then repeatedly nudges each constrained pair back
// toward its rest length. PBD is unconditionally stable (it corrects positions, never adds unbounded
// force), so cloth hangs, ropes swing, and pinned points hold — all deterministically. Pure CPU math,
// header-only; unit-tests exactly (a stretched link relaxes to rest, a pinned chain hangs without
// stretching, energy stays bounded). Collision against the rigid world is a follow-up.
namespace maz::game {

struct SoftParticle {
    math::vec3 pos{0.0f};
    math::vec3 prev{0.0f};  // previous position (Verlet velocity is pos - prev)
    float invMass = 1.0f;   // 0 => pinned (immovable)
};

struct DistanceConstraint {
    int a = 0;
    int b = 0;
    float rest = 1.0f;       // target separation
    float stiffness = 1.0f;  // [0,1]; 1 = fully rigid per iteration
};

class SoftBody {
  public:
    std::vector<SoftParticle> particles;
    std::vector<DistanceConstraint> constraints;
    math::vec3 gravity{0.0f, -9.81f, 0.0f};
    float damping = 0.01f; // per-step velocity damping in [0,1)
    int iterations = 8;    // constraint-projection passes per step

    int addParticle(const math::vec3& p, float invMass = 1.0f) {
        SoftParticle sp;
        sp.pos = p;
        sp.prev = p;
        sp.invMass = invMass;
        particles.push_back(sp);
        return static_cast<int>(particles.size()) - 1;
    }
    void pin(int idx) {
        if (idx >= 0 && idx < static_cast<int>(particles.size())) {
            particles[static_cast<std::size_t>(idx)].invMass = 0.0f;
        }
    }
    // Link two particles. rest < 0 => use their current separation as the rest length.
    int addConstraint(int a, int b, float stiffness = 1.0f, float rest = -1.0f) {
        DistanceConstraint c;
        c.a = a;
        c.b = b;
        c.stiffness = stiffness;
        if (rest >= 0.0f) {
            c.rest = rest;
        } else {
            const math::vec3 d = pos(b) - pos(a);
            c.rest = std::sqrt(glm::dot(d, d));
        }
        constraints.push_back(c);
        return static_cast<int>(constraints.size()) - 1;
    }

    // Advance one fixed step: Verlet integrate under gravity, then project constraints.
    void step(float dt) {
        const float keep = 1.0f - (damping < 0.0f ? 0.0f : (damping > 1.0f ? 1.0f : damping));
        for (SoftParticle& p : particles) {
            if (p.invMass <= 0.0f) {
                p.prev = p.pos; // pinned: velocity stays zero
                continue;
            }
            const math::vec3 vel = (p.pos - p.prev) * keep;
            const math::vec3 next = p.pos + vel + gravity * (dt * dt);
            p.prev = p.pos;
            p.pos = next;
        }
        for (int it = 0; it < iterations; ++it) {
            for (const DistanceConstraint& c : constraints) {
                projectDistance(c);
            }
        }
    }

    // Total kinetic-ish measure (sum of squared Verlet velocities) — a bounded quantity for tests.
    float velocityEnergy() const {
        float e = 0.0f;
        for (const SoftParticle& p : particles) {
            const math::vec3 v = p.pos - p.prev;
            e += glm::dot(v, v);
        }
        return e;
    }

    const math::vec3& pos(int i) const { return particles[static_cast<std::size_t>(i)].pos; }

  private:
    void projectDistance(const DistanceConstraint& c) {
        SoftParticle& a = particles[static_cast<std::size_t>(c.a)];
        SoftParticle& b = particles[static_cast<std::size_t>(c.b)];
        const float w = a.invMass + b.invMass;
        if (w <= 0.0f) {
            return; // both pinned
        }
        const math::vec3 delta = b.pos - a.pos;
        const float len = std::sqrt(glm::dot(delta, delta));
        if (len < 1e-8f) {
            return; // coincident; direction undefined this pass
        }
        const math::vec3 dir = delta / len;
        const float correction = (len - c.rest) * c.stiffness;
        // Distribute the correction by inverse mass (heavier particles move less).
        a.pos += dir * (a.invMass / w) * correction;
        b.pos -= dir * (b.invMass / w) * correction;
    }
};

} // namespace maz::game
