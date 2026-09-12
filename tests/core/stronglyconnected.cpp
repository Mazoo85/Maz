// tests/core/stronglyconnected.cpp — verifies Tarjan SCC grouping (core::stronglyConnectedComponents).
// Ground truths: mutually-reachable nodes share a component and unrelated ones don't; a pure cycle is one
// component; a DAG yields all singletons (count == N); the classic two-SCC example groups correctly;
// componentOf partitions every node exactly once; the condensation is acyclic and components come out in
// reverse-topological order (a component appears before the ones that point into it); self-loops, isolated
// nodes, the adjacency overload, and degenerate inputs are handled. Checked against the SCC definition.
#include "maz/core/StronglyConnected.hpp"

#include <cstdio>
#include <utility>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::core;

int main() {
    // --- 1. A pure 3-cycle is a single component; unrelated node is its own. ---
    {
        // 0->1->2->0 (one SCC), plus isolated node 3.
        const SccResult r = stronglyConnectedComponents(4, {{0, 1}, {1, 2}, {2, 0}});
        CHECK(r.components.size() == 2, "3-cycle + isolated -> 2 components");
        CHECK(sameComponent(r, 0, 1) && sameComponent(r, 1, 2), "cycle nodes share a component");
        CHECK(!sameComponent(r, 0, 3), "isolated node is separate");
    }

    // --- 2. A DAG yields all singletons. ---
    {
        const SccResult r = stronglyConnectedComponents(4, {{0, 1}, {0, 2}, {1, 3}, {2, 3}});
        CHECK(r.components.size() == 4, "acyclic graph -> N singleton components");
        for (const auto& c : r.components) CHECK(c.size() == 1, "each DAG component is a single node");
    }

    // --- 3. The classic two-SCC example. ---
    {
        // SCC A = {0,1,2} (0->1->2->0), SCC B = {3,4} (3->4->3), with a bridge 2->3 (A depends on B).
        const std::vector<std::pair<int, int>> edges = {{0, 1}, {1, 2}, {2, 0}, {2, 3}, {3, 4}, {4, 3}};
        const SccResult r = stronglyConnectedComponents(5, edges);
        CHECK(r.components.size() == 2, "two SCCs found");
        CHECK(sameComponent(r, 0, 2) && sameComponent(r, 3, 4), "each cycle is grouped");
        CHECK(!sameComponent(r, 2, 3), "the two SCCs stay separate");
        // Reverse-topological order: the bridge 2->3 means A's component index > B's component index
        // (the sink component B is emitted first).
        CHECK(r.componentOf[2] > r.componentOf[3], "reverse-topological: sink SCC {3,4} comes before {0,1,2}");
    }

    // --- 4. componentOf partitions every node exactly once and matches components[]. ---
    {
        const SccResult r = stronglyConnectedComponents(5, {{0, 1}, {1, 0}, {2, 3}, {3, 4}});
        for (int v = 0; v < 5; ++v) {
            const int id = r.componentOf[static_cast<std::size_t>(v)];
            CHECK(id >= 0 && id < static_cast<int>(r.components.size()), "componentOf in range");
            const auto& c = r.components[static_cast<std::size_t>(id)];
            bool found = false;
            for (int m : c) if (m == v) found = true;
            CHECK(found, "node listed in its own component");
        }
    }

    // --- 5. Reverse-topological guarantee holds for every cross-component edge. ---
    {
        // A chain of SCCs: {0,1} -> {2,3} -> {4}. Each edge should go to an equal-or-earlier component index.
        const std::vector<std::pair<int, int>> edges = {{0, 1}, {1, 0}, {1, 2}, {2, 3}, {3, 2}, {3, 4}};
        const SccResult r = stronglyConnectedComponents(5, edges);
        for (const auto& e : edges) {
            const int ca = r.componentOf[static_cast<std::size_t>(e.first)];
            const int cb = r.componentOf[static_cast<std::size_t>(e.second)];
            CHECK(ca >= cb, "edge points to an equal or earlier (sink-ward) component");
        }
    }

    // --- 6. Self-loop is its own single-node SCC (a node reachable from itself). ---
    {
        const SccResult r = stronglyConnectedComponents(2, {{0, 0}});
        CHECK(r.components.size() == 2, "self-loop node + isolated node -> 2 components");
        CHECK(sameComponent(r, 0, 0) && !sameComponent(r, 0, 1), "self-loop groups only itself");
    }

    // --- 7. Adjacency-list overload agrees; degenerate & defensive cases. ---
    {
        std::vector<std::vector<int>> succ(3);
        succ[0] = {1};
        succ[1] = {2};
        succ[2] = {0}; // 0->1->2->0 one SCC
        const SccResult r = stronglyConnectedComponents(succ);
        CHECK(r.components.size() == 1 && r.components[0].size() == 3, "adjacency overload finds the single SCC");

        CHECK(stronglyConnectedComponents(0, {}).components.empty(), "empty graph -> no components");
        const SccResult one = stronglyConnectedComponents(1, {});
        CHECK(one.components.size() == 1 && one.components[0][0] == 0, "single node -> one singleton");
        // Out-of-range endpoints ignored.
        const SccResult oob = stronglyConnectedComponents(2, {{0, 9}, {5, 1}});
        CHECK(oob.components.size() == 2, "out-of-range edges ignored -> 2 singletons");
    }

    if (g_fail == 0) {
        std::printf("stronglyconnected: OK — cycle, DAG, two-SCC, partition, reverse-topo, self-loop, overload.\n");
        return 0;
    }
    std::printf("stronglyconnected: %d failure(s).\n", g_fail);
    return 1;
}
