#pragma once

#include <cstddef>
#include <queue>
#include <utility>
#include <vector>

// maz::core::topologicalSort — order the nodes of a directed graph so every "must come before" edge points
// forward. The workhorse behind dependency resolution: a tech tree or skill tree whose nodes unlock in a
// legal order, a crafting chain (smelt ore before forging the blade), quest/prerequisite gating, asset or
// scene build order, and any "run these tasks respecting their dependencies" scheduler. If the graph has a
// cycle (a circular prerequisite that can never be satisfied) it says so and reports the nodes trapped in /
// downstream of the cycle, which is exactly the diagnostic a designer needs to find the bad edge. Uses
// Kahn's algorithm and always returns the LEXICOGRAPHICALLY SMALLEST valid order (ties broken by lowest node
// index), so the result is fully deterministic. Godot ships no general topological sort. Header-only, std-only.
namespace maz::core {

struct TopoResult {
    bool ok = false;                 // true iff the graph is a DAG (an ordering exists)
    std::vector<int> order;          // a valid topological order (lexicographically smallest) when ok
    std::vector<int> remaining;      // nodes left unordered — those in or downstream of a cycle — when !ok
};

namespace detail {

// Kahn's algorithm over an adjacency list (successors[a] = nodes that must come AFTER a). A min-heap of
// currently-source (in-degree 0) nodes yields the lexicographically smallest order. Multi-edges and
// self-loops are handled naturally: a self-loop keeps a node's in-degree above 0, so it is reported as a cycle.
inline TopoResult kahn(const std::vector<std::vector<int>>& successors) {
    const int n = static_cast<int>(successors.size());
    std::vector<int> indegree(static_cast<std::size_t>(n), 0);
    for (const std::vector<int>& outs : successors)
        for (int v : outs)
            if (v >= 0 && v < n) ++indegree[static_cast<std::size_t>(v)];

    std::priority_queue<int, std::vector<int>, std::greater<int>> ready; // smallest index first
    for (int i = 0; i < n; ++i)
        if (indegree[static_cast<std::size_t>(i)] == 0) ready.push(i);

    TopoResult r;
    r.order.reserve(static_cast<std::size_t>(n));
    while (!ready.empty()) {
        const int u = ready.top();
        ready.pop();
        r.order.push_back(u);
        for (int v : successors[static_cast<std::size_t>(u)]) {
            if (v < 0 || v >= n) continue;
            if (--indegree[static_cast<std::size_t>(v)] == 0) ready.push(v);
        }
    }

    if (static_cast<int>(r.order.size()) == n) {
        r.ok = true;
    } else {
        r.ok = false;
        r.order.clear(); // no complete ordering exists; don't hand back a partial one
        for (int i = 0; i < n; ++i)
            if (indegree[static_cast<std::size_t>(i)] > 0) r.remaining.push_back(i);
    }
    return r;
}

} // namespace detail

// Topologically sort `nodeCount` nodes (indices [0, nodeCount)) given directed "before" edges: each pair
// (a, b) means a must come before b. Isolated nodes with no edges are included. Edge endpoints outside
// [0, nodeCount) are ignored (defensive). Returns the lexicographically smallest order, or ok=false with the
// cycle-trapped nodes in `remaining`.
inline TopoResult topologicalSort(int nodeCount, const std::vector<std::pair<int, int>>& edges) {
    if (nodeCount < 0) nodeCount = 0;
    std::vector<std::vector<int>> successors(static_cast<std::size_t>(nodeCount));
    for (const auto& e : edges) {
        if (e.first >= 0 && e.first < nodeCount && e.second >= 0 && e.second < nodeCount)
            successors[static_cast<std::size_t>(e.first)].push_back(e.second);
    }
    return detail::kahn(successors);
}

// Adjacency-list overload: successors[a] lists the nodes that must come after a. The node count is the size
// of `successors`.
inline TopoResult topologicalSort(const std::vector<std::vector<int>>& successors) {
    return detail::kahn(successors);
}

// Convenience: does this "before"-edge graph contain a cycle?
inline bool hasCycle(int nodeCount, const std::vector<std::pair<int, int>>& edges) {
    return !topologicalSort(nodeCount, edges).ok;
}

} // namespace maz::core
