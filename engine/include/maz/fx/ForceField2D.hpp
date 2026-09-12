#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::fx {

// Composable 2D force field for particles & gameplay — Godot's GPUParticlesAttractor2D family plus a
// wind zone and drag, generalized into one reusable resource. The existing fx::ParticleSystem carries a
// single hard-wired attractor; this is a *set* of attractors/repulsors (each with a position, a signed
// strength, an influence radius, a falloff curve, and an optional tangential SWIRL for vortices), on top
// of a uniform directional WIND and a global linear DRAG. It exposes the pure force query `accelAt(pos,
// vel)` and a deterministic semi-implicit-Euler `step()` integrator over a particle array, so it drives
// gravity wells, black holes, wind tunnels, and orbiting swarms — and, being pure math with no GPU/RNG,
// it unit-tests exactly and renders a golden-stable swirl.
//
// Honest scope: this is a CPU point/vector force model. It does NOT implement Godot's texture-baked
// vector-field attractors, 3D attractors, or the full turbulence-noise process; those remain follow-ups.

struct Attractor2D {
    enum Falloff { Constant, Linear, InverseSquare };

    math::vec2 pos{0.0f, 0.0f};
    float strength = 0.0f;                // + pulls toward pos, - pushes away
    float radius = 0.0f;                  // influence radius; <= 0 means unbounded
    int falloff = InverseSquare;          // how strength scales with distance
    float swirl = 0.0f;                   // tangential accel around pos (+ = CCW), for vortices
};

struct FieldParticle {
    math::vec2 pos{0.0f, 0.0f};
    math::vec2 vel{0.0f, 0.0f};
};

class ForceField2D {
public:
    math::vec2 wind{0.0f, 0.0f}; // uniform acceleration added everywhere
    float drag = 0.0f;           // per-second fraction of velocity shed (0 = none)
    std::vector<Attractor2D> attractors;

    Attractor2D& addAttractor(math::vec2 pos, float strength, float radius = 0.0f,
                              int falloff = Attractor2D::InverseSquare, float swirl = 0.0f) {
        Attractor2D a;
        a.pos = pos;
        a.strength = strength;
        a.radius = radius;
        a.falloff = falloff;
        a.swirl = swirl;
        attractors.push_back(a);
        return attractors.back();
    }

    void clear() {
        attractors.clear();
        wind = math::vec2(0.0f, 0.0f);
        drag = 0.0f;
    }

    // Net acceleration at a point (excludes drag, which depends on the integrator's dt). `vel` is unused
    // by the point model but kept in the signature so velocity-dependent fields can slot in later.
    math::vec2 accelAt(math::vec2 pos, math::vec2 vel = math::vec2(0.0f)) const {
        (void)vel;
        math::vec2 a = wind;
        for (const Attractor2D& at : attractors) {
            const math::vec2 d = at.pos - pos;
            const float dist2 = d.x * d.x + d.y * d.y;
            const float dist = std::sqrt(dist2);
            if (at.radius > 0.0f && dist > at.radius) {
                continue; // outside influence
            }
            if (dist < 1e-4f) {
                continue; // at the singularity: no direction
            }
            const math::vec2 dir(d.x / dist, d.y / dist);

            float f = at.strength;
            switch (at.falloff) {
            case Attractor2D::Constant:
                break;
            case Attractor2D::Linear:
                if (at.radius > 0.0f) {
                    f *= 1.0f - dist / at.radius; // 1 at centre -> 0 at the edge
                }
                break;
            case Attractor2D::InverseSquare:
            default: {
                // Clamp the denominator so a particle sitting on the well doesn't launch to infinity.
                const float denom = dist2 < 1.0f ? 1.0f : dist2;
                f = at.strength / denom;
                break;
            }
            }

            a += dir * f;
            if (at.swirl != 0.0f) {
                // Perpendicular (rotate dir +90deg) gives an orbiting push; scale by the same radial
                // falloff weight so the vortex fades with distance too.
                float w = 1.0f;
                if (at.falloff == Attractor2D::Linear && at.radius > 0.0f) {
                    w = 1.0f - dist / at.radius;
                } else if (at.falloff == Attractor2D::InverseSquare) {
                    const float denom = dist2 < 1.0f ? 1.0f : dist2;
                    w = 1.0f / denom;
                }
                const math::vec2 perp(-dir.y, dir.x);
                a += perp * (at.swirl * w);
            }
        }
        return a;
    }

    // Advance every particle by dt using semi-implicit Euler (integrate velocity, then position), with
    // optional substeps for stability under strong fields. Drag is applied as exponential-ish per-step
    // decay. Deterministic given the same inputs.
    void step(std::vector<FieldParticle>& particles, float dt, int substeps = 1) const {
        if (substeps < 1) {
            substeps = 1;
        }
        const float h = dt / static_cast<float>(substeps);
        for (int s = 0; s < substeps; ++s) {
            for (FieldParticle& p : particles) {
                const math::vec2 a = accelAt(p.pos, p.vel);
                p.vel += a * h;
                if (drag > 0.0f) {
                    float k = drag * h;
                    if (k > 1.0f) {
                        k = 1.0f;
                    }
                    p.vel -= p.vel * k;
                }
                p.pos += p.vel * h;
            }
        }
    }
};

} // namespace maz::fx
