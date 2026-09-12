#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// maz::core::AhoCorasick — find EVERY occurrence of MANY search strings inside a text in a SINGLE pass over
// that text (Aho & Corasick, 1975). The naive approach — loop each of k patterns and scan the whole n-char
// text — costs O(n*k) and rescans the same characters k times; Aho-Corasick builds one automaton from all
// the patterns up front and then sweeps the text once in O(n + total matches), no matter how many patterns
// there are. It is the standard engine behind a profanity/word filter, chat slash-command detection,
// dialogue keyword triggers ("the player mentioned the king AND the sword"), search highlighting, and
// content moderation dictionaries — anywhere you must watch a stream of text for a whole vocabulary at once.
// The construction is a trie of the patterns plus "failure" links: when a match breaks mid-way, the failure
// link jumps to the longest proper suffix that is still a live prefix of some pattern, so the scan never
// backs up. Header-only, std-only, deterministic (matches are returned in scan order — by end position,
// then by pattern index). Godot has no multi-pattern matcher; this is well beyond String.find.
namespace maz::core {

class AhoCorasick {
public:
    struct Match {
        std::size_t begin;   // start index in the text (inclusive)
        std::size_t end;     // one past the last matched char (begin + pattern length)
        int pattern;         // index of the matched pattern, in the order added
    };

    AhoCorasick() { m_nodes.emplace_back(); } // node 0 is the root

    // Add a search pattern; returns its index (0-based, in insertion order). Empty patterns are ignored
    // (they would "match" everywhere) and return -1. Call before build(); adding after build() is undefined.
    int addPattern(std::string_view pat) {
        if (pat.empty()) {
            return -1;
        }
        const int id = static_cast<int>(m_lengths.size());
        int cur = 0;
        for (char ch : pat) {
            const auto c = static_cast<unsigned char>(ch);
            auto it = m_nodes[static_cast<std::size_t>(cur)].next.find(c);
            if (it == m_nodes[static_cast<std::size_t>(cur)].next.end()) {
                const int nxt = static_cast<int>(m_nodes.size());
                m_nodes.emplace_back();
                m_nodes[static_cast<std::size_t>(cur)].next.emplace(c, nxt);
                cur = nxt;
            } else {
                cur = it->second;
            }
        }
        m_nodes[static_cast<std::size_t>(cur)].outputs.push_back(id);
        m_lengths.push_back(pat.size());
        return id;
    }

    // Build the failure links. Must be called once after all patterns are added and before matching.
    void build() {
        std::queue<int> q;
        // Depth-1 nodes fail to the root.
        for (auto& [c, child] : m_nodes[0].next) {
            (void)c;
            m_nodes[static_cast<std::size_t>(child)].fail = 0;
            q.push(child);
        }
        while (!q.empty()) {
            const int u = q.front();
            q.pop();
            // Merge outputs reachable via the failure link so a node reports every pattern ending here.
            const int f = m_nodes[static_cast<std::size_t>(u)].fail;
            const auto& fout = m_nodes[static_cast<std::size_t>(f)].outputs;
            auto& uout = m_nodes[static_cast<std::size_t>(u)].outputs;
            uout.insert(uout.end(), fout.begin(), fout.end());
            std::sort(uout.begin(), uout.end());
            uout.erase(std::unique(uout.begin(), uout.end()), uout.end());
            for (auto& [c, v] : m_nodes[static_cast<std::size_t>(u)].next) {
                m_nodes[static_cast<std::size_t>(v)].fail = gotoState(m_nodes[static_cast<std::size_t>(u)].fail, c);
                q.push(v);
            }
        }
        m_built = true;
    }

    // Every occurrence of every pattern in `text`, in scan order (ascending end, then ascending pattern).
    std::vector<Match> findAll(std::string_view text) const {
        std::vector<Match> out;
        if (!m_built) {
            return out;
        }
        int cur = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const auto c = static_cast<unsigned char>(text[i]);
            cur = gotoState(cur, c);
            for (int id : m_nodes[static_cast<std::size_t>(cur)].outputs) {
                const std::size_t len = m_lengths[static_cast<std::size_t>(id)];
                out.push_back(Match{i + 1 - len, i + 1, id});
            }
        }
        return out;
    }

    // Does the text contain at least one of the patterns? (Stops at the first hit.)
    bool containsAny(std::string_view text) const {
        if (!m_built) {
            return false;
        }
        int cur = 0;
        for (char ch : text) {
            cur = gotoState(cur, static_cast<unsigned char>(ch));
            if (!m_nodes[static_cast<std::size_t>(cur)].outputs.empty()) {
                return true;
            }
        }
        return false;
    }

    // Number of distinct search positions matched (total match count, patterns may overlap).
    std::size_t countMatches(std::string_view text) const { return findAll(text).size(); }

    std::size_t patternCount() const { return m_lengths.size(); }

private:
    struct Node {
        std::unordered_map<unsigned char, int> next; // trie edges
        std::vector<int> outputs;                    // pattern ids ending at (or via failure links, through) here
        int fail = 0;                                // failure link
    };

    // Follow trie edge `c` from `state`, sliding down failure links when there's no edge (root stays put).
    int gotoState(int state, unsigned char c) const {
        for (;;) {
            const auto& node = m_nodes[static_cast<std::size_t>(state)];
            auto it = node.next.find(c);
            if (it != node.next.end()) {
                return it->second;
            }
            if (state == 0) {
                return 0;
            }
            state = node.fail;
        }
    }

    std::vector<Node> m_nodes;
    std::vector<std::size_t> m_lengths; // pattern lengths, indexed by pattern id
    bool m_built = false;
};

} // namespace maz::core
