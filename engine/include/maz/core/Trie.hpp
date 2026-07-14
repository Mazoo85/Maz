#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>  // std::unique_ptr
#include <cstddef>
#include <algorithm>  // std::sort

#include "maz/core/Assert.hpp"

namespace maz::core {

// A byte-oriented prefix tree (trie) — insert words, then contains (exact-word
// membership), startsWith (is there any word with this prefix), and
// collect(prefix) (all words sharing a prefix, sorted — for autocomplete). size
// counts DISTINCT words; insert is idempotent (re-inserting a word does not grow
// size). Operates on raw bytes (an unordered_map<char,...> keyed by byte value —
// NOT Unicode-aware). The command/asset-path autocomplete companion to the iter29
// console/CVars. NOT thread-safe.
//
// A compressed/radix trie and a longest-prefix-match are future refinements.
class Trie {
public:
    // Walk/create a node per character, marking the terminal node as a word. Idempotent:
    // re-inserting the same word does NOT grow size. An empty word marks m_root.isWord
    // (a valid edge — the empty string is a word you can contains()/collect()).
    void insert(std::string_view word) {
        Node* node = &m_root;
        for (char c : word) {
            std::unique_ptr<Node>& child = node->children[c];
            if (!child) {
                child = std::make_unique<Node>();
            }
            node = child.get();
        }
        if (!node->isWord) {
            node->isWord = true;
            ++m_size;
        }
    }

    // True only if the EXACT word was inserted (a terminal word, not merely a prefix).
    bool contains(std::string_view word) const {
        const Node* node = findNode(word);
        return node != nullptr && node->isWord;
    }

    // True if the full prefix path exists (regardless of isWord — any word has this
    // prefix). Empty prefix -> true (every trie has the empty prefix).
    bool startsWith(std::string_view prefix) const {
        return findNode(prefix) != nullptr;
    }

    // All words that start with `prefix`, SORTED lexicographically (the child map is
    // unordered, so the result is sorted for determinism). A non-existent prefix path or
    // a prefix with no matches -> {}. Empty prefix -> all words. The prefix node itself
    // is included if it is a word.
    std::vector<std::string> collect(std::string_view prefix) const {
        std::vector<std::string> out;
        const Node* node = findNode(prefix);
        if (node == nullptr) {
            return out;
        }
        std::string current(prefix);
        collectFrom(*node, current, out);
        std::sort(out.begin(), out.end());
        return out;
    }

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    void clear() {
        m_root.children.clear();
        m_root.isWord = false;
        m_size = 0;
    }

private:
    struct Node {
        std::unordered_map<char, std::unique_ptr<Node>> children;
        bool isWord = false;
    };

    // Walk the nodes for path `s` (no insertion — const .find); returns the node at the
    // end of the path or nullptr if any char is missing.
    const Node* findNode(std::string_view s) const {
        const Node* node = &m_root;
        for (char c : s) {
            auto it = node->children.find(c);
            if (it == node->children.end()) {
                return nullptr;
            }
            node = it->second.get();
        }
        return node;
    }

    // DFS from `node`: if it is a word push `current`, then recurse into each child,
    // extending/restoring `current` per edge char.
    void collectFrom(const Node& node, std::string& current, std::vector<std::string>& out) const {
        if (node.isWord) {
            out.push_back(current);
        }
        for (const auto& [ch, child] : node.children) {
            current.push_back(ch);
            collectFrom(*child, current, out);
            current.pop_back();
        }
    }

    Node m_root;             // root by value — its children map holds the first-character branches
    std::size_t m_size = 0;  // count of DISTINCT words inserted
};

} // namespace maz::core
