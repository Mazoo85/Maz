#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace maz::scene {

// Node groups — Godot's SceneTree groups (add_to_group / get_nodes_in_group / call_group / is_in_group).
// A group is a named tag you attach to any node; the registry answers "give me every node tagged X"
// (find all enemies, all save-points, everything to pause) and "run this on every node in X" — without
// the caller keeping and maintaining its own lists. Ids are plain integers, so this layers over
// ecs::World entities, scene::TransformGraph nodes, or an app's own handles. Membership is unique and
// kept in INSERTION ORDER, so queries and broadcasts are deterministic under the fixed timestep.
// A reverse index (node -> its groups) makes "which groups is this in?" and whole-node removal cheap.
// Header-only, no engine dependencies.

using GroupNode = std::uint32_t;

class GroupRegistry {
public:
    // Add `node` to `group`; no-op if already a member. Returns true if it was newly added.
    bool add(GroupNode node, const std::string& group) {
        std::vector<GroupNode>& members = m_groups[group];
        if (std::find(members.begin(), members.end(), node) != members.end()) {
            return false;
        }
        members.push_back(node);
        m_nodeGroups[node].push_back(group);
        return true;
    }

    // Remove `node` from `group`. Returns true if it was a member. Empty groups are dropped.
    bool remove(GroupNode node, const std::string& group) {
        auto git = m_groups.find(group);
        if (git == m_groups.end()) {
            return false;
        }
        std::vector<GroupNode>& members = git->second;
        auto it = std::find(members.begin(), members.end(), node);
        if (it == members.end()) {
            return false;
        }
        members.erase(it);
        if (members.empty()) {
            m_groups.erase(git);
        }
        eraseNodeGroup(node, group);
        return true;
    }

    // Remove `node` from every group it belongs to — call when the node is destroyed.
    void removeNode(GroupNode node) {
        auto nit = m_nodeGroups.find(node);
        if (nit == m_nodeGroups.end()) {
            return;
        }
        const std::vector<std::string> groups = nit->second; // copy: we mutate m_groups below
        for (const std::string& g : groups) {
            auto git = m_groups.find(g);
            if (git != m_groups.end()) {
                std::vector<GroupNode>& members = git->second;
                members.erase(std::remove(members.begin(), members.end(), node), members.end());
                if (members.empty()) {
                    m_groups.erase(git);
                }
            }
        }
        m_nodeGroups.erase(nit);
    }

    bool isInGroup(GroupNode node, const std::string& group) const {
        auto git = m_groups.find(group);
        if (git == m_groups.end()) {
            return false;
        }
        return std::find(git->second.begin(), git->second.end(), node) != git->second.end();
    }

    // Members of `group` in insertion order (empty if the group has none).
    std::vector<GroupNode> nodesInGroup(const std::string& group) const {
        auto git = m_groups.find(group);
        return git == m_groups.end() ? std::vector<GroupNode>{} : git->second;
    }

    std::size_t groupSize(const std::string& group) const {
        auto git = m_groups.find(group);
        return git == m_groups.end() ? 0 : git->second.size();
    }

    // The groups `node` belongs to, in the order it joined them.
    std::vector<std::string> groupsOf(GroupNode node) const {
        auto nit = m_nodeGroups.find(node);
        return nit == m_nodeGroups.end() ? std::vector<std::string>{} : nit->second;
    }

    bool hasGroup(const std::string& group) const { return m_groups.find(group) != m_groups.end(); }
    std::size_t groupCount() const { return m_groups.size(); }

    // Call `fn(node)` for every node in `group`. Iterates a SNAPSHOT, so `fn` may freely add or remove
    // members (e.g. free the node) without invalidating the walk — Godot's call_group semantics.
    void call(const std::string& group, const std::function<void(GroupNode)>& fn) const {
        const std::vector<GroupNode> snapshot = nodesInGroup(group);
        for (GroupNode n : snapshot) {
            fn(n);
        }
    }

    void clear() {
        m_groups.clear();
        m_nodeGroups.clear();
    }

private:
    void eraseNodeGroup(GroupNode node, const std::string& group) {
        auto nit = m_nodeGroups.find(node);
        if (nit == m_nodeGroups.end()) {
            return;
        }
        std::vector<std::string>& gs = nit->second;
        gs.erase(std::remove(gs.begin(), gs.end(), group), gs.end());
        if (gs.empty()) {
            m_nodeGroups.erase(nit);
        }
    }

    std::unordered_map<std::string, std::vector<GroupNode>> m_groups;
    std::unordered_map<GroupNode, std::vector<std::string>> m_nodeGroups;
};

} // namespace maz::scene
