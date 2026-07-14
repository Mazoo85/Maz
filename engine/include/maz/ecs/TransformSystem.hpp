#pragma once

#include <cstdint>
#include <unordered_map>

#include "maz/ecs/World.hpp"
#include "maz/ecs/Components.hpp"
#include "maz/math/Transform.hpp"

// The ECS transform-propagation system — computes each entity's WorldTransform
// from its LocalTransform up the Parent chain. This is the ECS analog of Godot's
// per-frame transform pass, and of iter22 maz::scene::SceneGraph's world-transform
// propagation (which lives OUTSIDE the ECS as a standalone node hierarchy); this
// slice brings that same pass onto maz::ecs::World's LocalTransform/Parent/
// WorldTransform components.
//
// The catch is that the ECS stores components in dense (SparseSet) order, which is
// NOT hierarchy order — a child may be visited before its parent. So propagation
// recurses UP the parent chain with per-pass memoization: each entity's world is
// computed exactly once, and a parent is always resolved before its children in ANY
// iteration order.
//
// Compose order: world = parentWorld * local — the parent-world on the LEFT — which
// follows directly from maz::math::Transform::operator* (proven in iter22 SceneGraph):
// (parentWorld * local).xform(x) == parentWorld.xform(local.xform(x)).
//
// Participation: an entity participates iff it has a LocalTransform. A Parent that is
// null, dead, or lacking a LocalTransform is treated as a ROOT (world = own local);
// following through transform-less intermediates up to a transform-bearing ancestor is
// a future refinement.
//
// Idempotent: memoization is per-call and results overwrite, so running the pass
// repeatedly (or after any mutation) reconverges to the same WorldTransforms.
//
// Cycle-safe: Parent is EXPECTED acyclic (maz::ecs::setParent has no cycle guard, unlike
// SceneGraph::setParent), but a cycle IS constructible via setParent. A recursion-stack
// guard (inProgress) breaks it — an ancestor already on the stack is treated as a root —
// so the pass always terminates; the broken-cycle result is defined-but-arbitrary.
//
// Binds directly as a maz::ecs::SystemScheduler SystemFn (plain void(World&)). NOT
// thread-safe (same as World). Dirty-flag / incremental propagation (recomputing only
// changed subtrees, as SceneGraph does lazily) is a future refinement.

namespace maz::ecs {

namespace detail {

// Precondition: e has LocalTransform. memo: finalized worlds this pass (compute-once).
// inProgress: current recursion stack (cycle guard — an ancestor already on the stack is treated as a root).
inline maz::math::Transform propagateWorld(
    World& w, Entity e,
    std::unordered_map<std::uint32_t, maz::math::Transform>& memo,
    std::unordered_map<std::uint32_t, bool>& inProgress) {
    if (auto it = memo.find(e.index); it != memo.end()) { return it->second; }
    const maz::math::Transform local = w.get<LocalTransform>(e)->value;
    maz::math::Transform world = local;  // default: root (no valid transform-bearing parent)
    Parent* p = w.get<Parent>(e);
    if (p && !p->value.isNull() && w.alive(p->value)
        && w.has<LocalTransform>(p->value)
        && inProgress.find(p->value.index) == inProgress.end()) {
        inProgress[e.index] = true;
        const maz::math::Transform pw = propagateWorld(w, p->value, memo, inProgress);
        inProgress.erase(e.index);
        world = pw * local;  // parent-world * local (compose order proven in iter22 SceneGraph)
    }
    memo[e.index] = world;
    return world;
}

} // namespace detail

inline void propagateTransforms(World& w) {
    std::unordered_map<std::uint32_t, maz::math::Transform> memo;
    std::unordered_map<std::uint32_t, bool> inProgress;
    w.each<LocalTransform>([&](Entity e, LocalTransform&) {
        const maz::math::Transform world = detail::propagateWorld(w, e, memo, inProgress);
        if (WorldTransform* wt = w.get<WorldTransform>(e)) { wt->value = world; }
        else { w.add<WorldTransform>(e, WorldTransform{world}); }  // create-on-demand; writes the WorldTransform store, NOT the LocalTransform store being iterated -> safe
    });
}

} // namespace maz::ecs
