#pragma once

#include "maz/ecs/World.hpp"
#include "maz/math/Math.hpp"

#include <cstdint>
#include <string>
#include <vector>

// maz::ecs core components — the handful of engine-standard components every scene needs, so games
// don't reinvent "where is this / what is it called / what is it a kind of / who is its parent":
//
//   Transform      — local TRS (position, rotation quaternion, scale) + matrix()
//   WorldTransform — the cached world matrix propagated from the hierarchy
//   Parent         — a link to a parent entity (Godot's node parenting, ECS-style)
//   Name           — a human-readable identifier
//   Tag            — a 64-bit category bitmask (fast "is this an Enemy/Pickup/…" filtering)
//
// plus propagateTransforms(World&), the hierarchy system that turns local Transforms + Parent links
// into WorldTransforms parent-first (a topological pass, cycle-safe). These mirror Godot's Node3D /
// name / groups, but as plain data an ECS system operates on. Header-only, no GPU.
namespace maz::ecs {

// Local transform: the TRS a node owns relative to its parent (or the world if unparented).
struct Transform {
    math::vec3 position{0.0f};
    math::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // identity (w,x,y,z)
    math::vec3 scale{1.0f};

    math::mat4 matrix() const {
        math::mat4 m = glm::translate(math::mat4(1.0f), position);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
};

// Cached world-space matrix produced by propagateTransforms().
struct WorldTransform {
    math::mat4 matrix{1.0f};

    math::vec3 position() const { return math::vec3(matrix[3]); }
};

// A parent link. An entity with no Parent component (or Parent{kNull}) is a root.
struct Parent {
    Entity parent = kNull;
};

// A human-readable name.
struct Name {
    std::string value;
};

// A category bitmask — up to 64 independent tags. Godot groups, but O(1) and allocation-free.
struct Tag {
    uint64_t bits = 0;

    bool has(uint32_t index) const { return (bits & (uint64_t(1) << index)) != 0; }
    void set(uint32_t index) { bits |= (uint64_t(1) << index); }
    void clear(uint32_t index) { bits &= ~(uint64_t(1) << index); }
    bool anyOf(uint64_t mask) const { return (bits & mask) != 0; }
    bool allOf(uint64_t mask) const { return (bits & mask) == mask; }
};

// Propagate local Transforms to WorldTransforms, parent-first. An entity with a Parent whose target
// has a WorldTransform is multiplied into its parent's world matrix; roots (no Parent, or an
// unresolved/dead parent) use their local matrix directly. Runs in passes so any hierarchy depth
// resolves correctly regardless of iteration order; a stuck pass (a cycle) breaks rather than
// looping forever. Every entity that has a Transform ends up with a WorldTransform.
inline void propagateTransforms(World& world) {
    // Seed: ensure a WorldTransform slot exists and mark all as not-yet-resolved this frame.
    std::vector<Entity> pending;
    world.each<Transform>([&](Entity e, Transform&) {
        if (!world.has<WorldTransform>(e)) {
            world.add<WorldTransform>(e, WorldTransform{});
        }
        pending.push_back(e);
    });

    // done[i] flips true once pending[i]'s world matrix is final. World has no per-entity marker,
    // so we resolve iteratively over the pending list until no more progress is possible.
    std::vector<bool> done(pending.size(), false);

    auto indexOf = [&](Entity e) -> long {
        for (size_t i = 0; i < pending.size(); ++i)
            if (pending[i] == e)
                return static_cast<long>(i);
        return -1;
    };

    bool progressed = true;
    size_t remaining = pending.size();
    while (remaining > 0 && progressed) {
        progressed = false;
        for (size_t i = 0; i < pending.size(); ++i) {
            if (done[i])
                continue;
            const Entity e = pending[i];
            const Transform* local = world.get<Transform>(e);
            const Parent* par = world.get<Parent>(e);
            const Entity pe = par ? par->parent : kNull;

            if (pe == kNull || !world.has<Transform>(pe) || pe == e) {
                // Root (or parent isn't a transform node / self-parent): local == world.
                world.get<WorldTransform>(e)->matrix = local->matrix();
                done[i] = true;
                --remaining;
                progressed = true;
            } else {
                const long pi = indexOf(pe);
                if (pi >= 0 && done[static_cast<size_t>(pi)]) {
                    world.get<WorldTransform>(e)->matrix =
                        world.get<WorldTransform>(pe)->matrix * local->matrix();
                    done[i] = true;
                    --remaining;
                    progressed = true;
                }
            }
        }
    }
    // Any survivors are part of a cycle; give them their local matrix so they're at least defined.
    for (size_t i = 0; i < pending.size(); ++i) {
        if (!done[i]) {
            const Entity e = pending[i];
            world.get<WorldTransform>(e)->matrix = world.get<Transform>(e)->matrix();
        }
    }
}

} // namespace maz::ecs
