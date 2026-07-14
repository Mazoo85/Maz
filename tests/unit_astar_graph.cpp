// Unit tests for maz::ai::AStarGraph — the general weighted-graph A*/Dijkstra
// shortest-path finder (Godot AStar3D analog), complementing the iter31 grid A*.
// Exercises a linear graph (exact Dijkstra cost), a diamond least-cost pick,
// start==goal, unreachable nodes / disconnected components, a directed edge,
// A*-with-distance-weights matching Dijkstra optimality, path validity/endpoints,
// and determinism. Integer-weight costs are asserted exactly (Dijkstra mode);
// the float-weight A* block uses an approx tolerance. Pure C++, no GPU/display.

#include "maz/ai/AStarGraph.hpp"

#include <cstdio>
#include <vector>
#include <cmath>

using namespace maz::ai;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) {
    const float d = a >= b ? a - b : b - a;
    return d <= 1e-4f;
}

// A returned path is well-formed if it is non-empty with the right endpoints.
bool endpointsOk(const std::vector<AStarGraph::NodeId>& p,
                 AStarGraph::NodeId start, AStarGraph::NodeId goal) {
    return !p.empty() && p.front() == start && p.back() == goal;
}

} // namespace

int main() {
    using NodeId = AStarGraph::NodeId;

    // --- LINEAR GRAPH, DIJKSTRA-MODE EXACT ----------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        g.connect(0, 1, 1.0f);
        g.connect(1, 2, 1.0f);
        g.connect(2, 3, 1.0f);
        const std::vector<NodeId> p = g.findPath(0, 3, 0.0f);
        const std::vector<NodeId> expected{ 0, 1, 2, 3 };
        check(p == expected, "linear graph path == {0,1,2,3}");
        check(g.lastCost() == 3.0f, "linear graph cost == 3");
    }

    // --- DIAMOND LEAST-COST (unique optimal) --------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        g.connect(0, 1, 1.0f);
        g.connect(1, 3, 1.0f);
        g.connect(0, 2, 4.0f);
        g.connect(2, 3, 1.0f);
        const std::vector<NodeId> p = g.findPath(0, 3, 0.0f);
        const std::vector<NodeId> expected{ 0, 1, 3 };  // cost 2, not 0-2-3 cost 5
        check(p == expected, "diamond picks least-cost path {0,1,3}");
        check(g.lastCost() == 2.0f, "diamond cost == 2 (not 5)");
    }

    // --- START == GOAL -------------------------------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.connect(0, 1, 1.0f);
        const std::vector<NodeId> p = g.findPath(1, 1);
        check(p.size() == 1 && p.front() == 1, "start==goal is a single-node path");
        check(g.lastCost() == 0.0f, "start==goal cost == 0");
    }

    // --- UNREACHABLE (isolated node + disconnected components) --------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.connect(0, 1, 1.0f);
        g.addNode(9);  // no edges
        check(g.findPath(0, 9, 0.0f).empty(), "isolated node -> unreachable -> empty");
        check(g.lastCost() == 0.0f, "unreachable leaves lastCost 0");

        // Two disconnected components {0,1} and {2,3}.
        g.addNode(2);
        g.addNode(3);
        g.connect(2, 3, 1.0f);
        check(g.findPath(0, 3, 0.0f).empty(), "disconnected components -> empty");
    }

    // --- DIRECTED EDGE -------------------------------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.connect(0, 1, 1.0f, /*bidirectional=*/false);
        const std::vector<NodeId> fwd = g.findPath(0, 1, 0.0f);
        const std::vector<NodeId> expected{ 0, 1 };
        check(fwd == expected, "directed edge forward {0,1}");
        check(g.lastCost() == 1.0f, "directed edge forward cost == 1");
        check(g.findPath(1, 0, 0.0f).empty(), "directed edge has no reverse -> empty");
    }

    // --- A* WITH DISTANCE WEIGHTS == DIJKSTRA OPTIMAL -----------------------
    {
        // Positions axis-aligned so edge weights EQUAL endpoint distances (the
        // admissible case). Two equal-length routes 0->2 both cost 7.
        AStarGraph g;
        g.addNode(0, maz::math::vec3(0.0f, 0.0f, 0.0f));
        g.addNode(1, maz::math::vec3(3.0f, 0.0f, 0.0f));
        g.addNode(2, maz::math::vec3(3.0f, 4.0f, 0.0f));
        g.addNode(3, maz::math::vec3(0.0f, 4.0f, 0.0f));
        g.connect(0, 1, 3.0f);  // dist (0,0,0)-(3,0,0) = 3
        g.connect(1, 2, 4.0f);  // dist (3,0,0)-(3,4,0) = 4
        g.connect(2, 3, 3.0f);  // dist (3,4,0)-(0,4,0) = 3
        g.connect(0, 3, 4.0f);  // dist (0,0,0)-(0,4,0) = 4

        const std::vector<NodeId> aStar = g.findPath(0, 2, 1.0f);
        const float aStarCost = g.lastCost();
        const std::vector<NodeId> dijkstra = g.findPath(0, 2, 0.0f);
        const float dijkstraCost = g.lastCost();

        check(endpointsOk(aStar, 0, 2), "A* path endpoints 0..2");
        check(endpointsOk(dijkstra, 0, 2), "Dijkstra path endpoints 0..2");
        check(approx(aStarCost, dijkstraCost), "A* cost == Dijkstra cost (admissible heuristic)");
        check(approx(aStarCost, 7.0f), "A* cost == 7 (optimal route)");
    }

    // --- PATH VALIDITY + ENDPOINTS ------------------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.connect(0, 1, 2.0f);
        g.connect(1, 2, 2.0f);
        const std::vector<NodeId> p = g.findPath(0, 2, 0.0f);
        check(p.size() >= 1, "returned path has >= 1 node");
        check(endpointsOk(p, 0, 2), "path front==start, back==goal");
    }

    // --- DETERMINISM ---------------------------------------------------------
    {
        AStarGraph g;
        g.addNode(0);
        g.addNode(1);
        g.addNode(2);
        g.addNode(3);
        g.connect(0, 1, 1.0f);
        g.connect(1, 2, 1.0f);
        g.connect(2, 3, 1.0f);
        g.connect(0, 2, 3.0f);
        const std::vector<NodeId> a = g.findPath(0, 3, 0.0f);
        const float aCost = g.lastCost();
        const std::vector<NodeId> b = g.findPath(0, 3, 0.0f);
        const float bCost = g.lastCost();
        check(a == b, "same inputs -> identical path (deterministic)");
        check(aCost == bCost, "same inputs -> identical cost (deterministic)");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
