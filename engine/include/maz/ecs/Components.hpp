#pragma once

#include <vector>

#include "maz/ecs/World.hpp"
#include "maz/math/Transform.hpp"
#include "maz/core/StringId.hpp"

// The ECS's core/built-in component types + small free-function helpers over
// World — the Godot core-node-property analog (Transform, Hierarchy/Parent,
// Name, Tag). These are plain structs added via World::add<T> and thin helpers
// that read/write them. They compose the earlier slices: iter5
// maz::math::Transform (the transform math), iter7 maz::core::StringId (hashed
// names/tags), and iter20 maz::ecs::World (the entity/component runtime).
// LocalTransform/WorldTransform wrap maz::math::Transform so the authored-local
// vs computed-world roles are DISTINCT component types. The transform-propagation
// SYSTEM (LocalTransform+Parent -> WorldTransform) and a name->entity index are
// future refinements (not built here); maz::scene::SceneGraph already provides a
// standalone hierarchy with world-transform propagation. NOT thread-safe (same
// as World).

namespace maz::ecs {

// Authored local transform (the entity's transform relative to its parent, or to
// world if it has no Parent). maz::math::Transform is the transform math (iter5);
// LocalTransform/WorldTransform wrap it so the two roles are distinct component types.
struct LocalTransform { maz::math::Transform value = maz::math::Transform::identity(); };
// Computed world transform (a system would recompute this from LocalTransform + Parent;
// that propagation system is out of scope here — see maz::scene::SceneGraph for the
// standalone hierarchy version).
struct WorldTransform { maz::math::Transform value = maz::math::Transform::identity(); };
// A human-readable entity name, stored as a hashed StringId (iter7) — Godot's node name.
struct Name { maz::core::StringId id; };
// A classification tag (StringId) — e.g. "enemy", "pickup".
struct Tag  { maz::core::StringId id; };
// Parent link for an entity hierarchy. null() == no parent (a root).
struct Parent { Entity value = Entity::null(); };

// Return the FIRST entity whose Name.id == name, or Entity::null() if none.
// O(n) linear scan over the Name store; a name->entity index is a future
// refinement. First match wins if names collide.
inline Entity findByName(World& w, maz::core::StringId name) {
    Entity found = Entity::null();
    w.each<Name>([&](Entity e, Name& n){
        if (found.isNull() && n.id == name) { found = e; }
    });
    return found;
}

// Upsert the Parent component so re-parenting never leaves a duplicate.
inline void setParent(World& w, Entity child, Entity parent) {
    if (Parent* p = w.get<Parent>(child)) { p->value = parent; }
    else { w.add<Parent>(child, Parent{parent}); }
}

// The child's parent, or Entity::null() if it has no Parent component.
inline Entity parentOf(World& w, Entity child) {
    if (Parent* p = w.get<Parent>(child)) { return p->value; }
    return Entity::null();
}

// Every entity whose Parent.value == parent. O(n) scan over the Parent store;
// order is unspecified (SparseSet dense order).
inline std::vector<Entity> childrenOf(World& w, Entity parent) {
    std::vector<Entity> out;
    w.each<Parent>([&](Entity e, Parent& p){ if (p.value == parent) { out.push_back(e); } });
    return out;
}

} // namespace maz::ecs
