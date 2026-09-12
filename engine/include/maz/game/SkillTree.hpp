#pragma once

// maz::game skill / talent tree — the point-buy progression graph behind "spend a talent point to
// unlock this node." Each node has a point cost and a max rank (buy it once, or several times for a
// stacking bonus); a node can be gated behind prerequisite nodes that must already be unlocked. The
// player earns points (e.g. from the M479 Leveling track) via `grantPoints`, and `unlock` spends them
// when the node is affordable and its prerequisites are met. `respec` refunds everything and resets
// the tree so the player can re-plan. This is the unlock *graph* only — what each node actually grants
// (a Stat bonus, an ability, etc.) is left to the game, keeping it distinct from the M476 Stat math.
// Godot ships no skill-tree system — it's hand-rolled every RPG — so this is a beyond-Godot gameplay
// utility. Header-only, std-only, deterministic.
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace maz::game {

class SkillTree {
public:
    SkillTree() = default;

    // Register a node. `cost` points are spent per rank (clamped >= 0); it can be purchased up to
    // `maxRank` times (clamped >= 1). Re-adding an id overwrites its cost/maxRank and resets its rank.
    void addNode(int id, int cost = 1, int maxRank = 1) {
        Node n;
        n.cost = cost < 0 ? 0 : cost;
        n.maxRank = maxRank < 1 ? 1 : maxRank;
        n.rank = 0;
        m_nodes[id] = n;
    }

    // Require `requiredNode` to be unlocked (rank >= 1) before `node` can be bought. Multiple
    // prerequisites are ANDed — every one must be met.
    void addPrerequisite(int node, int requiredNode) { m_prereqs[node].push_back(requiredNode); }

    // Add spendable points (negative values are ignored).
    void grantPoints(int n) {
        if (n > 0) m_available += n;
    }

    bool has(int id) const { return m_nodes.find(id) != m_nodes.end(); }
    int rankOf(int id) const {
        const auto it = m_nodes.find(id);
        return it == m_nodes.end() ? 0 : it->second.rank;
    }
    bool isUnlocked(int id) const { return rankOf(id) >= 1; }
    int maxRankOf(int id) const {
        const auto it = m_nodes.find(id);
        return it == m_nodes.end() ? 0 : it->second.maxRank;
    }
    int costOf(int id) const {
        const auto it = m_nodes.find(id);
        return it == m_nodes.end() ? 0 : it->second.cost;
    }

    // True when the node exists, isn't already maxed, is affordable, and all prerequisites are unlocked.
    bool canUnlock(int id) const {
        const auto it = m_nodes.find(id);
        if (it == m_nodes.end()) return false;
        const Node& n = it->second;
        if (n.rank >= n.maxRank) return false;
        if (m_available < n.cost) return false;
        return prereqsMet(id);
    }

    // Buy one rank of the node if `canUnlock`; spends its cost and returns true, else returns false.
    bool unlock(int id) {
        if (!canUnlock(id)) return false;
        Node& n = m_nodes[id];
        m_available -= n.cost;
        m_spent += n.cost;
        ++n.rank;
        return true;
    }

    int pointsAvailable() const { return m_available; }
    int pointsSpent() const { return m_spent; }

    // Refund every spent point and reset all ranks to 0 (the tree layout is kept).
    void respec() {
        m_available += m_spent;
        m_spent = 0;
        for (auto& kv : m_nodes) kv.second.rank = 0;
    }

    std::size_t nodeCount() const { return m_nodes.size(); }

    // Remove all nodes, prerequisites, and points.
    void clear() {
        m_nodes.clear();
        m_prereqs.clear();
        m_available = 0;
        m_spent = 0;
    }

private:
    struct Node {
        int cost = 1;
        int maxRank = 1;
        int rank = 0;
    };

    bool prereqsMet(int id) const {
        const auto it = m_prereqs.find(id);
        if (it == m_prereqs.end()) return true;
        for (const int req : it->second) {
            if (!isUnlocked(req)) return false;
        }
        return true;
    }

    std::unordered_map<int, Node> m_nodes;
    std::unordered_map<int, std::vector<int>> m_prereqs;
    int m_available = 0;
    int m_spent = 0;
};

} // namespace maz::game
