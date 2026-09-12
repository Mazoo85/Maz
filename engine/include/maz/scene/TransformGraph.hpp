#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::scene {

// A 2D transform hierarchy (scene graph): every node has a LOCAL transform (position, rotation,
// scale) relative to its parent, and update() propagates those into WORLD transforms parent-first.
// This is the structural backbone for composite objects — a turret on a tank, a moon around a planet,
// a hand on an arm, a health bar pinned to a unit: move or rotate the parent and the whole subtree
// follows. Composition is the standard decomposed TRS (world rotation = sum, world scale = product,
// world position = parent position + parent-rotated, parent-scaled local position), which is exact
// for uniform scale and the pragmatic norm for 2D engines. Header-only.

struct Transform2D {
    math::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians
    math::vec2 scale{1.0f, 1.0f};
};

class TransformGraph {
public:
    using Node = int;
    static constexpr Node kInvalid = -1;

    // Create a node with a parent (kInvalid = root) and a local transform. Children may be created in
    // any order relative to parents; update() resolves the dependency order.
    Node create(Node parent = kInvalid, const Transform2D& local = {}) {
        const Node n = static_cast<Node>(m_nodes.size());
        Nd nd;
        nd.local = local;
        nd.parent = parent;
        m_nodes.push_back(nd);
        return n;
    }

    std::size_t size() const { return m_nodes.size(); }

    void setParent(Node n, Node parent) { m_nodes[static_cast<size_t>(n)].parent = parent; }
    Node parent(Node n) const { return m_nodes[static_cast<size_t>(n)].parent; }

    // Local transform (mutable) — set position/rotation/scale here, then call update().
    Transform2D& local(Node n) { return m_nodes[static_cast<size_t>(n)].local; }
    const Transform2D& local(Node n) const { return m_nodes[static_cast<size_t>(n)].local; }

    // World transform (valid after update()).
    const Transform2D& world(Node n) const { return m_nodes[static_cast<size_t>(n)].world; }
    math::vec2 worldPosition(Node n) const { return m_nodes[static_cast<size_t>(n)].world.position; }
    float worldRotation(Node n) const { return m_nodes[static_cast<size_t>(n)].world.rotation; }
    math::vec2 worldScale(Node n) const { return m_nodes[static_cast<size_t>(n)].world.scale; }

    // Map a point in node n's local space into world space (uses n's world transform).
    math::vec2 localToWorld(Node n, math::vec2 p) const {
        const Transform2D& w = m_nodes[static_cast<size_t>(n)].world;
        return w.position + rotate(math::vec2{w.scale.x * p.x, w.scale.y * p.y}, w.rotation);
    }

    // Recompute every node's world transform. Safe for any parent/child creation order (memoized
    // recursion), so long as the hierarchy is acyclic.
    void update() {
        for (Nd& nd : m_nodes) nd.computed = false;
        for (std::size_t i = 0; i < m_nodes.size(); ++i)
            computeWorld(static_cast<Node>(i));
    }

private:
    struct Nd {
        Transform2D local;
        Transform2D world;
        Node parent = kInvalid;
        bool computed = false;
    };
    std::vector<Nd> m_nodes;

    static math::vec2 rotate(math::vec2 v, float a) {
        const float c = std::cos(a), s = std::sin(a);
        return math::vec2{v.x * c - v.y * s, v.x * s + v.y * c};
    }

    const Transform2D& computeWorld(Node n) {
        Nd& nd = m_nodes[static_cast<size_t>(n)];
        if (nd.computed) return nd.world;
        nd.computed = true; // set first so a stray cycle terminates instead of recursing forever
        if (nd.parent == kInvalid) {
            nd.world = nd.local;
        } else {
            const Transform2D p = computeWorld(nd.parent); // copy: vector may reallocate? no, no growth here
            nd.world.rotation = p.rotation + nd.local.rotation;
            nd.world.scale = math::vec2{p.scale.x * nd.local.scale.x, p.scale.y * nd.local.scale.y};
            const math::vec2 scaled{p.scale.x * nd.local.position.x, p.scale.y * nd.local.position.y};
            nd.world.position = p.position + rotate(scaled, p.rotation);
        }
        return nd.world;
    }
};

} // namespace maz::scene
