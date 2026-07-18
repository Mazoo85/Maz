#pragma once

#include "maz/game/Physics3D.hpp"

#include <cmath>
#include <vector>

// maz::game ragdoll builder — Godot's PhysicalBone3D ragdoll: turn a skeleton (a list of bones, each a
// segment with a parent) into a jointed set of capsule rigid bodies. Each bone becomes a capsule
// oriented along its length; each non-root bone is tied to its parent by a cone-twist joint at the
// shared joint point, so the assembly swings and collapses like a limp body while staying connected.
// This composes the existing 3D physics (capsule bodies + cone-twist joints, M235) into the one helper
// games actually want. Pure CPU; unit-tests exactly (bodies + joints created, connectivity preserved
// under simulation, falls under gravity without exploding).
namespace maz::game {

struct RagdollBone {
    math::vec3 head{0.0f};  // the joint end (connects to the parent's tail)
    math::vec3 tail{0.0f};  // the free end
    float radius = 0.1f;
    float mass = 1.0f;
    int parent = -1;        // index of the parent bone, or -1 for the root
    float swingSpan = 0.7853982f; // cone half-angle for the joint to the parent (45 deg)
    float twistSpan = 0.5235988f; // twist limit (30 deg)
};

struct Ragdoll {
    std::vector<int> bodyIndex;  // world body index per bone
    std::vector<int> jointIndex; // world joint index per non-root bone (-1 for the root)
};

namespace detail {
// Shortest-arc rotation taking unit vector `from` onto unit vector `to`.
inline math::quat shortestArc(const math::vec3& from, const math::vec3& to) {
    const float d = glm::dot(from, to);
    if (d > 0.99999f) {
        return math::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (d < -0.99999f) {
        // 180 degrees: pick any axis perpendicular to `from`.
        math::vec3 axis = glm::cross(math::vec3(1, 0, 0), from);
        if (glm::dot(axis, axis) < 1e-6f) {
            axis = glm::cross(math::vec3(0, 0, 1), from);
        }
        axis = glm::normalize(axis);
        return math::quat(0.0f, axis.x, axis.y, axis.z);
    }
    const math::vec3 c = glm::cross(from, to);
    const float s = std::sqrt((1.0f + d) * 2.0f);
    const float inv = 1.0f / s;
    return glm::normalize(math::quat(s * 0.5f, c.x * inv, c.y * inv, c.z * inv));
}
} // namespace detail

// Build the ragdoll into `world`. Each bone is added as a capsule body (oriented along the bone) and,
// if it has a parent, joined to it with a cone-twist joint at the bone's head. Returns the created
// body/joint indices.
inline Ragdoll buildRagdoll(PhysicsWorld3D& world, const std::vector<RagdollBone>& bones) {
    Ragdoll out;
    out.bodyIndex.assign(bones.size(), -1);
    out.jointIndex.assign(bones.size(), -1);

    // Pass 1: bodies.
    for (std::size_t i = 0; i < bones.size(); ++i) {
        const RagdollBone& b = bones[i];
        const math::vec3 seg = b.tail - b.head;
        const float len = std::sqrt(glm::dot(seg, seg));
        const math::vec3 dir = len > 1e-6f ? seg / len : math::vec3(0, 1, 0);
        float halfHeight = len * 0.5f - b.radius;
        if (halfHeight < 0.0f) {
            halfHeight = 0.0f; // very short bone -> essentially a sphere-capped stub
        }
        const math::vec3 mid = (b.head + b.tail) * 0.5f;
        Body3D body = makeCapsule(mid, b.radius, halfHeight, b.mass);
        body.orientation = detail::shortestArc(math::vec3(0, 1, 0), dir); // capsule axis is +Y
        body.enableRotation();
        body.friction = 0.5f;
        out.bodyIndex[i] = world.add(body);
    }

    // Pass 2: cone-twist joints to parents (bodies must exist first).
    for (std::size_t i = 0; i < bones.size(); ++i) {
        const RagdollBone& b = bones[i];
        if (b.parent < 0 || b.parent >= static_cast<int>(bones.size())) {
            continue;
        }
        const int childBody = out.bodyIndex[i];
        const int parentBody = out.bodyIndex[static_cast<std::size_t>(b.parent)];
        const math::vec3 seg = b.tail - b.head;
        const float len = std::sqrt(glm::dot(seg, seg));
        const math::vec3 dir = len > 1e-6f ? seg / len : math::vec3(0, 1, 0);
        world.joints.push_back(makeConeTwistJoint3(
            parentBody, world.bodies[static_cast<std::size_t>(parentBody)], childBody,
            world.bodies[static_cast<std::size_t>(childBody)], b.head, dir, b.swingSpan, b.twistSpan));
        out.jointIndex[i] = static_cast<int>(world.joints.size()) - 1;
    }
    return out;
}

} // namespace maz::game
