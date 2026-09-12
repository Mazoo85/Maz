#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cstddef>
#include <vector>

// maz::anim::SpringBone — secondary motion for a chain of bones: tails, hair, antennae, ponytails, capes,
// dangling accessories, and anything that should JIGGLE and trail as the character moves rather than stay
// rigidly rigged. You drive the ROOT joint each frame (attach it to a real bone), and the rest of the chain
// follows with inertia and damping — lagging behind sudden motion, overshooting, then settling back into the
// rest pose. Each joint is a damped spring pulled toward where it would be if rigidly attached to its parent,
// and because a joint chases its parent's CURRENT (also-lagging) position, the motion propagates down the
// chain like a whip. Godot exposes this as its SpringBoneSimulator / jiggle modifiers; the engine had
// core::Spring (a single scalar/vector spring) and SoftBody (full physics) but no bone-chain jiggle. Stepped
// with sub-stepping so it stays stable at any frame rate. Header-only, pure, deterministic.
namespace maz::anim {

class SpringBone {
public:
    // Initialise from the chain's rest positions (world space). Joint 0 is the driven root; each later joint
    // remembers its offset from its parent, which is the pose the spring pulls it back toward.
    void init(const std::vector<math::vec3>& restPositions) {
        m_joints.clear();
        m_joints.reserve(restPositions.size());
        for (std::size_t i = 0; i < restPositions.size(); ++i) {
            Joint j;
            j.position = restPositions[i];
            j.velocity = math::vec3(0.0f, 0.0f, 0.0f);
            j.restOffset = (i == 0) ? math::vec3(0.0f, 0.0f, 0.0f) : restPositions[i] - restPositions[i - 1];
            m_joints.push_back(j);
        }
    }

    void setStiffness(float k) { m_stiffness = k; } // spring pull toward the rest pose (higher = snappier)
    void setDamping(float d) { m_damping = d; }     // velocity damping (higher = less wobble)
    void setGravity(const math::vec3& g) { m_gravity = g; }

    // Drive the root joint (joint 0) — attach this to the parent bone's world position each frame.
    void setRoot(const math::vec3& p) {
        if (!m_joints.empty()) m_joints[0].position = p;
    }

    // Advance the simulation by dt (sub-stepped for stability). The root stays where setRoot put it.
    void update(float dt) {
        if (dt <= 0.0f || m_joints.size() < 2) return;
        constexpr float kMaxStep = 1.0f / 240.0f;
        float remaining = dt;
        while (remaining > 0.0f) {
            const float h = remaining < kMaxStep ? remaining : kMaxStep;
            step(h);
            remaining -= h;
        }
    }

    // Snap the whole chain rigidly to the rest pose relative to the current root, clearing all velocity.
    void reset() {
        for (std::size_t i = 1; i < m_joints.size(); ++i)
            m_joints[i].position = m_joints[i - 1].position + m_joints[i].restOffset;
        for (Joint& j : m_joints) j.velocity = math::vec3(0.0f, 0.0f, 0.0f);
    }

    std::size_t size() const { return m_joints.size(); }
    const math::vec3& position(std::size_t i) const { return m_joints[i].position; }
    const math::vec3& velocity(std::size_t i) const { return m_joints[i].velocity; }

private:
    struct Joint {
        math::vec3 position{0.0f, 0.0f, 0.0f};
        math::vec3 velocity{0.0f, 0.0f, 0.0f};
        math::vec3 restOffset{0.0f, 0.0f, 0.0f};
    };

    void step(float h) {
        // Joint 0 is pinned (the driver). Each child springs toward parent + restOffset, using the parent's
        // already-updated position this step so motion propagates down the chain.
        for (std::size_t i = 1; i < m_joints.size(); ++i) {
            Joint& j = m_joints[i];
            const math::vec3 target = m_joints[i - 1].position + j.restOffset;
            const math::vec3 accel = (target - j.position) * m_stiffness - j.velocity * m_damping + m_gravity;
            j.velocity += accel * h;
            j.position += j.velocity * h;
        }
    }

    std::vector<Joint> m_joints;
    float m_stiffness = 30.0f;
    float m_damping = 5.0f;
    math::vec3 m_gravity{0.0f, 0.0f, 0.0f};
};

} // namespace maz::anim
