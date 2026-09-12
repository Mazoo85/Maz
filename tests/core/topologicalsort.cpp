// tests/core/topologicalsort.cpp — verifies DAG ordering (core::topologicalSort / hasCycle).
// Ground truths: every "before" edge points forward in the returned order; the order is the
// lexicographically-smallest valid one (deterministic); isolated nodes are included; a real dependency
// chain (crafting/tech tree) resolves; a cycle is detected and its trapped nodes are reported; a self-loop
// is a cycle; the adjacency-list overload agrees; empty and single-node graphs are handled; out-of-range
// edge endpoints are ignored. Checked against the topological-order definition. Pure CPU, deterministic.
#include "maz/core/TopologicalSort.hpp"

#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::core;

// Position of each node in the order; -1 if absent.
static std::unordered_map<int, int> positions(const std::vector<int>& order) {
    std::unordered_map<int, int> p;
    for (int i = 0; i < static_cast<int>(order.size()); ++i) p[order[static_cast<std::size_t>(i)]] = i;
    return p;
}

int main() {
    // --- 1. Every edge points forward; isolated node included; lexicographically smallest. ---
    {
        // 0->2, 1->2, 2->3, plus isolated node 4. Edge 2->0 absent; expect smallest order.
        const std::vector<std::pair<int, int>> edges = {{0, 2}, {1, 2}, {2, 3}};
        const TopoResult r = topologicalSort(5, edges);
        CHECK(r.ok, "acyclic graph orders");
        CHECK(r.order.size() == 5, "all 5 nodes present (incl. isolated 4)");
        const auto pos = positions(r.order);
        for (const auto& e : edges) CHECK(pos.at(e.first) < pos.at(e.second), "edge points forward");
        // Sources are 0, 1, 4 (in-degree 0). Smallest-first: 0, then 1, then... 2 needs both 0 and 1.
        // Heap trace: {0,1,4} -> pop 0 -> {1,4}; pop 1 -> 2 ready -> {2,4}; pop 2 -> 3 ready -> {3,4};
        // pop 3 -> {4}; pop 4. Order = [0,1,2,3,4].
        CHECK((r.order == std::vector<int>{0, 1, 2, 3, 4}), "lexicographically smallest order");
    }

    // --- 2. Determinism: the tie-break prefers lower indices. ---
    {
        // Only edge 2->0: sources {1,2,3}. Smallest-first: 1, 2 (frees 0), 0, 3.
        const TopoResult r = topologicalSort(4, {{2, 0}});
        CHECK(r.ok && (r.order == std::vector<int>{1, 2, 0, 3}), "deterministic smallest order with a dependency");
    }

    // --- 3. A realistic crafting chain resolves in dependency order. ---
    {
        // 0=ore, 1=coal, 2=ingot(needs ore+coal), 3=blade(needs ingot), 4=hilt, 5=sword(needs blade+hilt).
        const std::vector<std::pair<int, int>> deps = {{0, 2}, {1, 2}, {2, 3}, {3, 5}, {4, 5}};
        const TopoResult r = topologicalSort(6, deps);
        CHECK(r.ok, "crafting chain is acyclic");
        const auto pos = positions(r.order);
        for (const auto& e : deps) CHECK(pos.at(e.first) < pos.at(e.second), "prerequisite precedes result");
        CHECK(pos.at(2) > pos.at(0) && pos.at(2) > pos.at(1), "ingot after both ore and coal");
        CHECK(pos.at(5) > pos.at(3) && pos.at(5) > pos.at(4), "sword after blade and hilt");
    }

    // --- 4. A cycle is detected and its trapped nodes reported. ---
    {
        // 0->1->2->0 is a 3-cycle; node 3 is a clean tail off 2.
        const TopoResult r = topologicalSort(4, {{0, 1}, {1, 2}, {2, 0}, {2, 3}});
        CHECK(!r.ok, "cycle detected");
        CHECK(r.order.empty(), "no partial order handed back on failure");
        // 0,1,2 are trapped in the cycle; 3 is downstream of it (its only in-edge comes from cyclic 2).
        CHECK((r.remaining == std::vector<int>{0, 1, 2, 3}), "remaining lists cycle + downstream nodes");
        CHECK(hasCycle(4, {{0, 1}, {1, 2}, {2, 0}}), "hasCycle agrees on a pure 3-cycle");
    }

    // --- 5. A self-loop is a cycle. ---
    {
        const TopoResult r = topologicalSort(2, {{1, 1}});
        CHECK(!r.ok, "self-loop is a cycle");
        // Node 0 is a clean source; only node 1 is trapped.
        CHECK((r.remaining == std::vector<int>{1}), "only the self-looping node is trapped");
    }

    // --- 6. Adjacency-list overload agrees with the edge-list form. ---
    {
        // successors[a] = nodes after a. Same graph as test 1's edges.
        std::vector<std::vector<int>> succ(5);
        succ[0] = {2};
        succ[1] = {2};
        succ[2] = {3};
        const TopoResult r = topologicalSort(succ);
        CHECK(r.ok && (r.order == std::vector<int>{0, 1, 2, 3, 4}), "adjacency overload matches edge-list");
    }

    // --- 7. Degenerate & defensive cases. ---
    {
        CHECK(topologicalSort(0, {}).ok && topologicalSort(0, {}).order.empty(), "empty graph -> ok, empty");
        const TopoResult one = topologicalSort(1, {});
        CHECK(one.ok && one.order.size() == 1 && one.order[0] == 0, "single node -> [0]");
        // Out-of-range endpoints are ignored, not crashes.
        const TopoResult oob = topologicalSort(2, {{0, 1}, {5, 0}, {1, 9}});
        CHECK(oob.ok && (oob.order == std::vector<int>{0, 1}), "out-of-range edges ignored");
    }

    if (g_fail == 0) {
        std::printf("topologicalsort: OK — forward edges, lexicographic order, chain, cycle, self-loop, overload, edges.\n");
        return 0;
    }
    std::printf("topologicalsort: %d failure(s).\n", g_fail);
    return 1;
}
