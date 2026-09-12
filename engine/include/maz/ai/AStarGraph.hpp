#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>          // std::priority_queue
#include <algorithm>      // std::reverse
#include <cmath>          // std::sqrt (via glm::length)
#include <tuple>          // std::tie

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

// General weighted-graph shortest-path finder (Godot AStar3D analog). Register
// nodes (each with an optional 3D position used ONLY by the heuristic), connect
// them with non-negative weighted edges (directed or bidirectional), then
// findPath(start, goal, heuristicScale). heuristicScale scales a Euclidean-
// distance heuristic: 0 makes it pure Dijkstra (always optimal for non-negative
// weights); >0 is A* (optimal only when the heuristic is admissible — edge
// weights >= endpoint straight-line distance, e.g. weights ARE the node
// distances). Deterministic tie-break (lowest f, then lowest h, then lowest node
// id) so an unbroken run on the same inputs returns the same path. Unreachable
// goal -> empty path; start==goal -> single-node path. Complements iter31's grid
// AStar (AStarGrid.hpp). NOT thread-safe for mutation; findPath is const. Dynamic
// edge removal and point-disable are future refinements (not built here).

namespace maz::ai {

class AStarGraph {
  public:
    using NodeId = std::uint32_t;

    // Register a node. position is only used by the findPath heuristic; a zero
    // position + heuristicScale 0 degenerates the search to pure Dijkstra.
    void addNode(NodeId id, maz::math::vec3 position = maz::math::vec3(0.0f)) {
        MAZ_ASSERT(m_nodes.find(id) == m_nodes.end(), "AStarGraph::addNode: duplicate id");
        m_nodes.emplace(id, Node{ position, {} });
    }

    bool hasNode(NodeId id) const { return m_nodes.find(id) != m_nodes.end(); }

    // Connect a -> b with weight; if bidirectional also b -> a with the same
    // weight. This APPENDS an edge — it does NOT dedupe, so connecting the same
    // pair twice leaves two parallel edges (the search just sees both).
    void connect(NodeId a, NodeId b, float weight, bool bidirectional = true) {
        MAZ_ASSERT(hasNode(a) && hasNode(b), "AStarGraph::connect: both nodes must exist");
        MAZ_ASSERT(weight >= 0.0f, "AStarGraph::connect: negative weight");
        m_nodes.at(a).edges.push_back(Edge{ b, weight });
        if (bidirectional) {
            m_nodes.at(b).edges.push_back(Edge{ a, weight });
        }
    }

    void clear() {
        m_nodes.clear();
        m_lastCost = 0.0f;
    }

    std::size_t nodeCount() const { return m_nodes.size(); }

    // Total cost of the most recent findPath. Only meaningful when that call
    // returned a non-empty path or start==goal (0 for start==goal, and 0 for an
    // unreachable/empty result).
    float lastCost() const { return m_lastCost; }

    // Cost-optimal A* over the graph. See the file header for the full contract.
    std::vector<NodeId> findPath(NodeId start, NodeId goal, float heuristicScale = 1.0f) const {
        MAZ_ASSERT(hasNode(start) && hasNode(goal), "AStarGraph::findPath: start/goal must exist");
        MAZ_ASSERT(heuristicScale >= 0.0f, "AStarGraph::findPath: heuristicScale must be >= 0");

        m_lastCost = 0.0f;
        if (start == goal) { return { start }; }

        // Euclidean distance to the goal, scaled. heuristicScale==0 -> exactly
        // Dijkstra (always optimal for non-negative weights). For A* (scale>0) to
        // stay optimal the heuristic must be admissible: edge weights >= the
        // straight-line distance between endpoints (guaranteed when weights ARE
        // the node distances); with arbitrary weights use heuristicScale 0.
        const maz::math::vec3 goalPos = m_nodes.at(goal).position;
        const auto h = [this, goalPos, heuristicScale](NodeId n) -> float {
            return heuristicScale * glm::length(goalPos - m_nodes.at(n).position);
        };

        std::unordered_map<NodeId, float> g;
        std::unordered_map<NodeId, NodeId> came;
        std::unordered_set<NodeId> closed;
        g[start] = 0.0f;

        struct QNode { float f, h; NodeId id; };
        // Min-heap on (f, then h, then id): priority_queue is a max-heap, so the
        // comparator returns true when 'a' should be popped AFTER 'b'.
        struct QNodeCmp {
            bool operator()(const QNode& a, const QNode& b) const {
                return std::tie(a.f, a.h, a.id) > std::tie(b.f, b.h, b.id);
            }
        };
        std::priority_queue<QNode, std::vector<QNode>, QNodeCmp> open;
        {
            const float sh = h(start);
            open.push(QNode{ sh, sh, start });
        }

        bool reached = false;
        while (!open.empty()) {
            const QNode n = open.top();
            open.pop();
            if (closed.find(n.id) != closed.end()) { continue; }  // lazy deletion: stale entry
            closed.insert(n.id);
            if (n.id == goal) { reached = true; break; }

            const float gCurrent = g.at(n.id);
            for (const Edge& e : m_nodes.at(n.id).edges) {
                const float tentative = gCurrent + e.weight;
                const auto it = g.find(e.to);
                if (it == g.end() || tentative < it->second) {
                    g[e.to] = tentative;
                    came[e.to] = n.id;
                    const float hto = h(e.to);
                    open.push(QNode{ tentative + hto, hto, e.to });
                }
            }
        }

        if (!reached) { return {}; }  // unreachable: m_lastCost stays 0

        // Reconstruct start..goal by walking parents backward, then reverse.
        std::vector<NodeId> path;
        NodeId cur = goal;
        path.push_back(cur);
        while (cur != start) {
            cur = came.at(cur);
            path.push_back(cur);
        }
        std::reverse(path.begin(), path.end());
        m_lastCost = g.at(goal);
        return path;
    }

  private:
    struct Edge {
        NodeId to;
        float weight;
    };
    struct Node {
        maz::math::vec3 position;
        std::vector<Edge> edges;
    };

    std::unordered_map<NodeId, Node> m_nodes;
    mutable float m_lastCost = 0.0f;
};

} // namespace maz::ai
