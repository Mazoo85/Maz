#pragma once

#include "maz/math/Math.hpp"

#include <string>
#include <vector>

// maz::ui node-graph model — Godot's GraphEdit / GraphNode: the data structure behind visual scripting,
// the shader graph, and animation/blend trees. Nodes carry named input/output ports and a canvas
// position; connections wire an output port of one node to an input port of another. This is the pure
// graph brain: add/remove nodes, connect/disconnect ports with full validation (both endpoints exist,
// no self-links, no duplicate wires, and — for the acyclic graphs shader/visual-script use — no cycles),
// plus queries and a Kahn topological ordering for evaluation. No rendering here (the editor draws the
// boxes and wires); header-only + deterministic, so it unit-tests exactly.
namespace maz::ui {

struct GraphPort {
    std::string name;
    int typeTag = 0; // optional user type id for port-compatibility checks
};

struct GraphNode {
    std::string id;
    math::vec2 position{0.0f, 0.0f};
    std::vector<GraphPort> inputs;
    std::vector<GraphPort> outputs;
};

struct GraphConnection {
    std::string fromNode;
    int fromPort = 0; // output port index on fromNode
    std::string toNode;
    int toPort = 0; // input port index on toNode

    bool operator==(const GraphConnection& o) const {
        return fromNode == o.fromNode && fromPort == o.fromPort && toNode == o.toNode &&
               toPort == o.toPort;
    }
};

class GraphEdit {
  public:
    // When true (default), connect() refuses any wire that would introduce a cycle — the invariant
    // shader graphs and visual scripts rely on. Set false for free-form graphs.
    bool acyclic = true;

    // --- nodes ------------------------------------------------------------------------------------
    bool addNode(const GraphNode& n) {
        if (n.id.empty() || findNode(n.id) != nullptr) {
            return false; // ids must be unique and non-empty
        }
        m_nodes.push_back(n);
        return true;
    }
    const GraphNode* findNode(const std::string& id) const {
        for (const GraphNode& n : m_nodes) {
            if (n.id == id) {
                return &n;
            }
        }
        return nullptr;
    }
    GraphNode* findNode(const std::string& id) {
        for (GraphNode& n : m_nodes) {
            if (n.id == id) {
                return &n;
            }
        }
        return nullptr;
    }
    // Remove a node and every connection touching it.
    bool removeNode(const std::string& id) {
        const std::size_t before = m_nodes.size();
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i].id == id) {
                m_nodes.erase(m_nodes.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        if (m_nodes.size() == before) {
            return false;
        }
        std::vector<GraphConnection> kept;
        for (const GraphConnection& c : m_conns) {
            if (c.fromNode != id && c.toNode != id) {
                kept.push_back(c);
            }
        }
        m_conns.swap(kept);
        return true;
    }
    std::size_t nodeCount() const { return m_nodes.size(); }
    const std::vector<GraphNode>& nodes() const { return m_nodes; }

    // --- connections ------------------------------------------------------------------------------
    // Wire fromNode.output[fromPort] -> toNode.input[toPort]. Returns false if either endpoint is
    // invalid, it is a self-link, the wire already exists, or (when acyclic) it would form a cycle.
    bool connect(const std::string& fromNode, int fromPort, const std::string& toNode, int toPort) {
        const GraphNode* a = findNode(fromNode);
        const GraphNode* b = findNode(toNode);
        if (a == nullptr || b == nullptr) {
            return false;
        }
        if (fromNode == toNode) {
            return false; // no self-links
        }
        if (fromPort < 0 || fromPort >= static_cast<int>(a->outputs.size())) {
            return false;
        }
        if (toPort < 0 || toPort >= static_cast<int>(b->inputs.size())) {
            return false;
        }
        const GraphConnection c{fromNode, fromPort, toNode, toPort};
        for (const GraphConnection& e : m_conns) {
            if (e == c) {
                return false; // duplicate wire
            }
        }
        if (acyclic && wouldCreateCycle(fromNode, toNode)) {
            return false;
        }
        m_conns.push_back(c);
        return true;
    }
    bool disconnect(const std::string& fromNode, int fromPort, const std::string& toNode, int toPort) {
        const GraphConnection c{fromNode, fromPort, toNode, toPort};
        for (std::size_t i = 0; i < m_conns.size(); ++i) {
            if (m_conns[i] == c) {
                m_conns.erase(m_conns.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }
    bool isConnected(const std::string& fromNode, int fromPort, const std::string& toNode,
                     int toPort) const {
        const GraphConnection c{fromNode, fromPort, toNode, toPort};
        for (const GraphConnection& e : m_conns) {
            if (e == c) {
                return true;
            }
        }
        return false;
    }
    std::size_t connectionCount() const { return m_conns.size(); }
    const std::vector<GraphConnection>& connections() const { return m_conns; }

    // Would adding a node-level edge from->to close a loop? True iff a path to -> ... -> from exists.
    bool wouldCreateCycle(const std::string& fromNode, const std::string& toNode) const {
        if (fromNode == toNode) {
            return true;
        }
        std::vector<std::string> stack{toNode};
        std::vector<std::string> seen;
        while (!stack.empty()) {
            const std::string cur = stack.back();
            stack.pop_back();
            if (cur == fromNode) {
                return true;
            }
            bool visited = false;
            for (const std::string& s : seen) {
                if (s == cur) {
                    visited = true;
                    break;
                }
            }
            if (visited) {
                continue;
            }
            seen.push_back(cur);
            for (const GraphConnection& c : m_conns) {
                if (c.fromNode == cur) {
                    stack.push_back(c.toNode);
                }
            }
        }
        return false;
    }

    // Kahn topological order of node ids (dependencies first). Empty if the graph has a cycle.
    std::vector<std::string> topologicalOrder() const {
        std::vector<std::string> ids;
        std::vector<int> indeg;
        ids.reserve(m_nodes.size());
        indeg.reserve(m_nodes.size());
        for (const GraphNode& n : m_nodes) {
            ids.push_back(n.id);
            indeg.push_back(0);
        }
        auto idx = [&](const std::string& id) -> int {
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] == id) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        for (const GraphConnection& c : m_conns) {
            const int ti = idx(c.toNode);
            if (ti >= 0) {
                ++indeg[static_cast<std::size_t>(ti)];
            }
        }
        std::vector<std::string> ready;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (indeg[i] == 0) {
                ready.push_back(ids[i]);
            }
        }
        std::vector<std::string> order;
        while (!ready.empty()) {
            const std::string n = ready.front();
            ready.erase(ready.begin());
            order.push_back(n);
            for (const GraphConnection& c : m_conns) {
                if (c.fromNode == n) {
                    const int ti = idx(c.toNode);
                    if (ti >= 0 && --indeg[static_cast<std::size_t>(ti)] == 0) {
                        ready.push_back(c.toNode);
                    }
                }
            }
        }
        if (order.size() != m_nodes.size()) {
            return {}; // cycle -> no valid ordering
        }
        return order;
    }

  private:
    std::vector<GraphNode> m_nodes;
    std::vector<GraphConnection> m_conns;
};

} // namespace maz::ui
