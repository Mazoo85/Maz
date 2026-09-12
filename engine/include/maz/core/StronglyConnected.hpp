#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

// maz::core::stronglyConnectedComponents — group the nodes of a directed graph into its strongly-connected
// components (SCCs): maximal sets where every node can reach every other. The companion to topologicalSort
// (M637): where that orders a graph with no cycles, this one FINDS the cycles and clusters them. Uses:
// collapsing a tangle of mutually-dependent quests / dialogue states / crafting recipes into one unit,
// detecting circular references in a scene or resource graph and reporting exactly which nodes form each
// loop, condensing a messy dependency graph into a clean DAG (each SCC becomes one super-node), and
// deadlock/liveness analysis on a state machine. Tarjan's algorithm, run ITERATIVELY (an explicit work
// stack, not recursion) so it is safe on very deep graphs. Components come out in REVERSE TOPOLOGICAL order
// of the condensation — a "sink" component (one that depends on nothing further) appears before the
// components that point into it — and each component's node list is sorted ascending for determinism.
// A node with no cycle is its own singleton component. Godot ships no SCC primitive. Header-only, std-only.
namespace maz::core {

struct SccResult {
    std::vector<std::vector<int>> components; // each sorted ascending; in reverse-topological order
    std::vector<int> componentOf;             // componentOf[node] = index into `components`
};

namespace detail {

inline SccResult tarjan(const std::vector<std::vector<int>>& successors) {
    const int n = static_cast<int>(successors.size());
    SccResult result;
    result.componentOf.assign(static_cast<std::size_t>(n), -1);

    std::vector<int> index(static_cast<std::size_t>(n), -1); // discovery index, -1 = unvisited
    std::vector<int> low(static_cast<std::size_t>(n), 0);     // low-link
    std::vector<char> onStack(static_cast<std::size_t>(n), 0);
    std::vector<int> comp(static_cast<std::size_t>(n), 0);    // scan cursor per node (which child is next)
    std::vector<int> sccStack;                                // Tarjan's component stack
    int counter = 0;

    // Explicit DFS: each frame is a node whose children we walk via its `comp` cursor.
    std::vector<int> work;
    for (int s = 0; s < n; ++s) {
        if (index[static_cast<std::size_t>(s)] != -1) continue;
        work.push_back(s);
        while (!work.empty()) {
            const int v = work.back();
            const std::size_t vv = static_cast<std::size_t>(v);
            if (index[vv] == -1) { // first time we touch v
                index[vv] = low[vv] = counter++;
                onStack[vv] = 1;
                sccStack.push_back(v);
            }
            bool recursed = false;
            const std::vector<int>& outs = successors[vv];
            while (comp[vv] < static_cast<int>(outs.size())) {
                const int w = outs[static_cast<std::size_t>(comp[vv])];
                ++comp[vv];
                if (w < 0 || w >= n) continue; // ignore out-of-range edges
                const std::size_t ww = static_cast<std::size_t>(w);
                if (index[ww] == -1) {          // tree edge: descend into w
                    work.push_back(w);
                    recursed = true;
                    break;
                } else if (onStack[ww]) {       // back/cross edge to an active node
                    low[vv] = std::min(low[vv], index[ww]);
                }
            }
            if (recursed) continue;

            if (low[vv] == index[vv]) {         // v is the root of an SCC: pop it off
                std::vector<int> component;
                for (;;) {
                    const int w = sccStack.back();
                    sccStack.pop_back();
                    onStack[static_cast<std::size_t>(w)] = 0;
                    component.push_back(w);
                    if (w == v) break;
                }
                std::sort(component.begin(), component.end());
                const int id = static_cast<int>(result.components.size());
                for (int node : component) result.componentOf[static_cast<std::size_t>(node)] = id;
                result.components.push_back(std::move(component));
            }
            work.pop_back();
            if (!work.empty()) { // propagate v's low-link up to its DFS parent
                const std::size_t pp = static_cast<std::size_t>(work.back());
                low[pp] = std::min(low[pp], low[vv]);
            }
        }
    }
    return result;
}

} // namespace detail

// SCCs of a directed graph with `nodeCount` nodes (indices [0, nodeCount)) and directed edges (a, b) meaning
// a -> b. Out-of-range endpoints are ignored (defensive). See the header note for the ordering guarantees.
inline SccResult stronglyConnectedComponents(int nodeCount, const std::vector<std::pair<int, int>>& edges) {
    if (nodeCount < 0) nodeCount = 0;
    std::vector<std::vector<int>> successors(static_cast<std::size_t>(nodeCount));
    for (const auto& e : edges) {
        if (e.first >= 0 && e.first < nodeCount && e.second >= 0 && e.second < nodeCount)
            successors[static_cast<std::size_t>(e.first)].push_back(e.second);
    }
    return detail::tarjan(successors);
}

// Adjacency-list overload: successors[a] lists the nodes a points to. Node count is successors.size().
inline SccResult stronglyConnectedComponents(const std::vector<std::vector<int>>& successors) {
    return detail::tarjan(successors);
}

// Convenience: are nodes a and b in the same strongly-connected component (mutually reachable)?
inline bool sameComponent(const SccResult& scc, int a, int b) {
    if (a < 0 || b < 0 || a >= static_cast<int>(scc.componentOf.size()) ||
        b >= static_cast<int>(scc.componentOf.size()))
        return false;
    return scc.componentOf[static_cast<std::size_t>(a)] == scc.componentOf[static_cast<std::size_t>(b)];
}

} // namespace maz::core
