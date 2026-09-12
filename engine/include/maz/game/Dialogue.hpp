#pragma once

#include <cstddef>
#include <string>
#include <vector>

// maz::game branching dialogue system — the conversation graph behind NPC talk, cutscene lines, and
// "[1] Accept / [2] Decline" prompts. A DialogueTree is a set of nodes, each with a speaker, a line of
// text, and either a list of player CHOICES (each pointing to the next node) or a single auto-advance
// `next` link for a straight run of lines. A DialogueRunner walks the tree: it exposes the current line
// and its choices, `choose(i)` follows a branch, and `advance()` steps a choice-less line forward, ending
// the conversation when a link points to -1. The tree is pure data (one tree can drive many NPCs at once
// via separate runners). Godot ships no dialogue system — games hand-roll it every time or reach for a
// third-party addon — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

struct DialogueChoice {
    std::string text;
    int next = -1; // node id to jump to when picked; -1 ends the conversation
};

struct DialogueNode {
    int id = -1;
    std::string speaker;
    std::string text;
    std::vector<DialogueChoice> choices; // player-facing branches; empty = a linear line
    int next = -1;                       // auto-advance target when there are no choices; -1 ends
};

class DialogueTree {
public:
    DialogueTree() = default;

    // Add a node; returns its id (sequential from 0).
    int addNode(std::string speaker, std::string text) {
        const int id = static_cast<int>(m_nodes.size());
        m_nodes.push_back(DialogueNode{id, std::move(speaker), std::move(text), {}, -1});
        return id;
    }

    // Add a player choice on `nodeId` that jumps to `next` (-1 ends). No-op for a bad node id.
    void addChoice(int nodeId, std::string text, int next) {
        DialogueNode* n = node(nodeId);
        if (n != nullptr) n->choices.push_back(DialogueChoice{std::move(text), next});
    }

    // Set a choice-less node's auto-advance target (-1 ends). No-op for a bad node id.
    void setNext(int nodeId, int next) {
        DialogueNode* n = node(nodeId);
        if (n != nullptr) n->next = next;
    }

    std::size_t nodeCount() const { return m_nodes.size(); }
    const std::vector<DialogueNode>& nodes() const { return m_nodes; }

    DialogueNode* node(int id) {
        if (id < 0 || id >= static_cast<int>(m_nodes.size())) return nullptr;
        return &m_nodes[static_cast<std::size_t>(id)];
    }
    const DialogueNode* node(int id) const {
        if (id < 0 || id >= static_cast<int>(m_nodes.size())) return nullptr;
        return &m_nodes[static_cast<std::size_t>(id)];
    }

private:
    std::vector<DialogueNode> m_nodes;
};

class DialogueRunner {
public:
    explicit DialogueRunner(const DialogueTree& tree) : m_tree(&tree) {}

    // Begin at `nodeId`. An invalid id starts finished.
    void start(int nodeId = 0) { m_current = m_tree->node(nodeId) != nullptr ? nodeId : -1; }

    bool isFinished() const { return current() == nullptr; }

    // The line currently being shown, or nullptr once the conversation has ended.
    const DialogueNode* current() const { return m_tree->node(m_current); }

    // Number of player choices on the current line (0 for a linear or ended line).
    std::size_t choiceCount() const {
        const DialogueNode* n = current();
        return n == nullptr ? 0 : n->choices.size();
    }

    // Pick choice `index` on the current line and follow it. Returns true if the conversation continues
    // (a valid node follows), false if it ended or the pick was invalid (no state change on invalid).
    bool choose(std::size_t index) {
        const DialogueNode* n = current();
        if (n == nullptr || index >= n->choices.size()) return false;
        return goTo(n->choices[index].next);
    }

    // Step a choice-less line forward via its `next` link. Returns true if the conversation continues.
    // A no-op returning false when the current line has choices (the player must choose) or has ended.
    bool advance() {
        const DialogueNode* n = current();
        if (n == nullptr || !n->choices.empty()) return false;
        return goTo(n->next);
    }

private:
    bool goTo(int nodeId) {
        m_current = m_tree->node(nodeId) != nullptr ? nodeId : -1;
        return m_current >= 0;
    }

    const DialogueTree* m_tree;
    int m_current = -1;
};

} // namespace maz::game
