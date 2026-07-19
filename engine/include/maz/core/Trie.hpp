#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// maz::core::Trie — a prefix tree over strings: stores a set of words so that "is this exact word
// present?", "is any word here starting with this prefix?", and "give me every word under this
// prefix (sorted)" are all answered in time proportional to the query length, independent of how many
// words are stored. The natural structure behind developer-console / chat command autocomplete,
// dictionary word validation (spelling, word games), profanity/keyword filtering, and prefix-indexed
// lookups — none of which Godot provides a primitive for. Each node caches a subtree word count, so
// prefix existence and counts stay O(len) even after erases. Header-only, std-only; children are kept
// in a std::map so traversals emit words in lexicographic order.
namespace maz::core {

class Trie {
public:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    Trie() { m_nodes.emplace_back(); } // node 0 is the root

    void clear() {
        m_nodes.clear();
        m_nodes.emplace_back();
        m_size = 0;
    }

    std::size_t size() const { return m_size; } // number of distinct words
    bool empty() const { return m_size == 0; }

    // Insert a word. Returns false if it was already present.
    bool insert(const std::string& word) {
        if (contains(word)) {
            return false;
        }
        std::size_t cur = 0;
        ++m_nodes[0].count;
        for (char c : word) {
            auto it = m_nodes[cur].next.find(c);
            std::size_t nxt;
            if (it == m_nodes[cur].next.end()) {
                nxt = m_nodes.size();
                m_nodes.emplace_back();        // may reallocate; re-index cur afterwards
                m_nodes[cur].next[c] = nxt;
            } else {
                nxt = it->second;
            }
            cur = nxt;
            ++m_nodes[cur].count;
        }
        m_nodes[cur].word = true;
        ++m_size;
        return true;
    }

    // Remove a word. Returns false if it was not present. Nodes are kept but subtree counts drop, so
    // startsWith()/countWithPrefix() reflect the removal exactly.
    bool erase(const std::string& word) {
        if (!contains(word)) {
            return false;
        }
        std::size_t cur = 0;
        --m_nodes[0].count;
        for (char c : word) {
            cur = m_nodes[cur].next.at(c);
            --m_nodes[cur].count;
        }
        m_nodes[cur].word = false;
        --m_size;
        return true;
    }

    // Exact-word membership.
    bool contains(const std::string& word) const {
        const std::size_t n = findNode(word);
        return n != npos && m_nodes[n].word;
    }

    // Is any stored word prefixed by `prefix`? (Empty prefix -> true iff any words exist.)
    bool startsWith(const std::string& prefix) const {
        const std::size_t n = findNode(prefix);
        return n != npos && m_nodes[n].count > 0;
    }

    // How many stored words are prefixed by `prefix`.
    std::size_t countWithPrefix(const std::string& prefix) const {
        const std::size_t n = findNode(prefix);
        return n == npos ? 0 : m_nodes[n].count;
    }

    // Every stored word starting with `prefix`, in lexicographic order (autocomplete).
    std::vector<std::string> collectWithPrefix(const std::string& prefix) const {
        std::vector<std::string> out;
        const std::size_t n = findNode(prefix);
        if (n == npos || m_nodes[n].count == 0) {
            return out;
        }
        std::string buffer = prefix;
        collect(n, buffer, out);
        return out;
    }

private:
    struct Node {
        std::map<char, std::size_t> next; // child char -> node index
        std::size_t count = 0;            // words in this subtree (this node included)
        bool word = false;                // a word ends here
    };

    // Node index reached by walking `s` from the root, or npos if the path breaks.
    std::size_t findNode(const std::string& s) const {
        std::size_t cur = 0;
        for (char c : s) {
            const auto it = m_nodes[cur].next.find(c);
            if (it == m_nodes[cur].next.end()) {
                return npos;
            }
            cur = it->second;
        }
        return cur;
    }

    void collect(std::size_t node, std::string& buffer, std::vector<std::string>& out) const {
        if (m_nodes[node].word) {
            out.push_back(buffer);
        }
        for (const auto& [c, child] : m_nodes[node].next) {
            if (m_nodes[child].count > 0) {
                buffer.push_back(c);
                collect(child, buffer, out);
                buffer.pop_back();
            }
        }
    }

    std::vector<Node> m_nodes;
    std::size_t m_size = 0;
};

} // namespace maz::core
