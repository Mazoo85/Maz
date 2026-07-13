#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm> // std::remove

#include "maz/math/Transform.hpp"
#include "maz/core/Assert.hpp"

// A scene graph / node transform hierarchy with LAZY world-transform propagation via
// dirty flags. Each node carries a local maz::math::Transform; its world transform is
// world = parentWorld * local, recomputed on demand and cached. Any setLocal/setParent
// marks the node's whole subtree dirty. This upholds the dirty invariant: a clean node's
// ancestors are all clean, so getWorld's recursion up the parent chain is correct.
//
// Composes maz::math::Transform (iter5). Node destruction/recycling is a future
// refinement (not built here). NOT thread-safe.

namespace maz::scene {

using NodeId = std::uint32_t;

class SceneGraph {
public:
    static constexpr NodeId kInvalidNode = 0xFFFFFFFFu;

    struct Node {
        maz::math::Transform local;
        maz::math::Transform world;
        std::uint32_t parent;
        std::vector<std::uint32_t> children;
        bool dirty;
    };

    SceneGraph() {
        // Root: world IS identity and already clean (parent = kInvalidNode).
        m_nodes.push_back(Node{ maz::math::Transform::identity(), maz::math::Transform::identity(), kInvalidNode, {}, false });
    }

    static constexpr NodeId root() { return 0; }

    NodeId createNode(NodeId parent = 0) {
        MAZ_ASSERT(parent < m_nodes.size(), "createNode: parent out of range");
        const NodeId id = static_cast<std::uint32_t>(m_nodes.size());
        // dirty=true so the first getWorld computes it. push_back may realloc; use indices only (do NOT hold a Node& across it).
        m_nodes.push_back(Node{ maz::math::Transform::identity(), maz::math::Transform::identity(), parent, {}, true });
        m_nodes[parent].children.push_back(id);  // AFTER the node push_back
        return id;
    }

    void setLocal(NodeId n, const maz::math::Transform& t) {
        MAZ_ASSERT(n < m_nodes.size(), "setLocal: node out of range");
        m_nodes[n].local = t;
        markSubtreeDirty(n);
    }

    const maz::math::Transform& getLocal(NodeId n) const {
        MAZ_ASSERT(n < m_nodes.size(), "getLocal: node out of range");
        return m_nodes[n].local;
    }

    const maz::math::Transform& getWorld(NodeId n) {
        MAZ_ASSERT(n < m_nodes.size(), "getWorld: node out of range");
        if (m_nodes[n].dirty) {
            const std::uint32_t p = m_nodes[n].parent;
            // pw is a COPY (value): getWorld(p) returns a const ref into m_nodes and we are about to WRITE m_nodes[n].world — copy first to avoid aliasing. (getWorld never push_backs, so no realloc.)
            const maz::math::Transform pw = (p == kInvalidNode) ? maz::math::Transform::identity() : getWorld(p);
            m_nodes[n].world = pw * m_nodes[n].local;  // parent-world * local (compose order confirmed from Transform::operator*)
            m_nodes[n].dirty = false;
        }
        return m_nodes[n].world;
    }

    void setParent(NodeId n, NodeId newParent) {
        MAZ_ASSERT(n != root(), "setParent: cannot reparent the root");
        MAZ_ASSERT(n < m_nodes.size() && newParent < m_nodes.size(), "setParent: out of range");
        // Cycle guard: n must not equal or be an ancestor of newParent. Walk up from newParent to kInvalidNode.
        for (std::uint32_t a = newParent; a != kInvalidNode; a = m_nodes[a].parent) {
            MAZ_ASSERT(a != n, "setParent: would create a cycle");
        }
        // Remove n from its old parent's children (erase-remove idiom).
        auto& oldChildren = m_nodes[m_nodes[n].parent].children;
        oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), n), oldChildren.end());
        m_nodes[n].parent = newParent;
        m_nodes[newParent].children.push_back(n);
        markSubtreeDirty(n);
    }

    std::size_t nodeCount() const { return m_nodes.size(); }

private:
    void markSubtreeDirty(std::uint32_t n) {
        m_nodes[n].dirty = true;
        // Recurse. markSubtreeDirty never push_backs to m_nodes, so no realloc: the ref into children stays valid.
        for (std::uint32_t c : m_nodes[n].children) { markSubtreeDirty(c); }
    }

    std::vector<Node> m_nodes;
};

} // namespace maz::scene
