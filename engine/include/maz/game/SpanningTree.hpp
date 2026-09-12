#pragma once

#include "maz/core/DisjointSet.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::game minimum spanning tree — Kruskal's algorithm over a weighted, undirected graph. Given a set
// of nodes and weighted edges, it returns the cheapest set of edges that keeps every node connected
// (with no cycles). This is the standard procedural-generation tool for connecting a scatter of dungeon
// rooms with the shortest total corridor length, laying out road/river/power networks, or clustering
// points — anywhere you need "link these up as cheaply as possible". If the graph is disconnected it
// returns the minimum spanning FOREST (one tree per component). Reuses core::DisjointSet (union-find) so
// each edge is accepted only when it joins two so-far-separate components. Godot ships no MST, so this is
// a beyond-Godot utility. Deterministic (ties broken by original edge order). Header-only, std-only.
namespace maz::game {

struct MstEdge {
    int a = 0;
    int b = 0;
    float weight = 0.0f;
};

struct MstResult {
    std::vector<MstEdge> edges;      // the chosen tree/forest edges
    float totalWeight = 0.0f;        // sum of chosen edge weights
    bool connected = false;          // true iff the result spans all nodes (a single tree)
    std::size_t componentCount = 0;  // number of connected components in the input graph
};

// Build the minimum spanning tree (or forest) of a graph with `nodeCount` nodes in [0, nodeCount) and
// the given weighted edges. Self-loops and out-of-range endpoints are ignored; parallel edges are fine
// (the lightest is chosen). O(E log E) from the edge sort.
inline MstResult minimumSpanningTree(int nodeCount, const std::vector<MstEdge>& edges) {
    MstResult out;
    if (nodeCount <= 0) {
        return out;
    }
    std::vector<std::size_t> order(edges.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t l, std::size_t r) { return edges[l].weight < edges[r].weight; });

    core::DisjointSet ds(static_cast<std::size_t>(nodeCount));
    const std::size_t target = static_cast<std::size_t>(nodeCount) - 1; // edges in a spanning tree
    for (std::size_t idx : order) {
        if (out.edges.size() == target) {
            break; // a full spanning tree is complete
        }
        const MstEdge& e = edges[idx];
        if (e.a < 0 || e.b < 0 || e.a >= nodeCount || e.b >= nodeCount || e.a == e.b) {
            continue;
        }
        if (ds.unite(static_cast<std::size_t>(e.a), static_cast<std::size_t>(e.b))) {
            out.edges.push_back(e);
            out.totalWeight += e.weight;
        }
    }
    out.componentCount = ds.count();
    out.connected = (out.edges.size() == target);
    return out;
}

} // namespace maz::game
