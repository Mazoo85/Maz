#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// maz::core::SuffixArray — index a string so that ANY substring can be located fast. A suffix array is the
// list of all the string's suffixes sorted alphabetically, stored as their start positions; because the
// suffixes are sorted, every occurrence of a search pattern forms one contiguous block, found by binary
// search in O(m log n) instead of scanning the whole text. It is the compact, cache-friendly cousin of a
// suffix tree and the backbone of substring search over large static text: searching a big log or script
// dump, autocomplete over a dictionary, dedup/longest-repeated-substring analysis, and building block for
// bioinformatics-style matching. Paired with the LCP (longest-common-prefix) array — the overlap between
// each adjacent pair of sorted suffixes — it also answers "what is the longest chunk that repeats?" directly.
// Built by prefix doubling. Header-only, std-only, deterministic. Godot has no text index; this is well
// beyond String.find for repeated queries on fixed text.
namespace maz::core {

class SuffixArray {
public:
    SuffixArray() = default;
    explicit SuffixArray(std::string text) { build(std::move(text)); }

    // Build the suffix array (and LCP array) for `text`.
    void build(std::string text) {
        m_text = std::move(text);
        const int n = static_cast<int>(m_text.size());
        m_sa.assign(static_cast<std::size_t>(n), 0);
        m_lcp.assign(static_cast<std::size_t>(n), 0);
        if (n == 0) {
            return;
        }
        std::vector<int> rank(static_cast<std::size_t>(n));
        std::vector<int> tmp(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            m_sa[static_cast<std::size_t>(i)] = i;
            rank[static_cast<std::size_t>(i)] = static_cast<unsigned char>(m_text[static_cast<std::size_t>(i)]);
        }
        for (int k = 1;; k <<= 1) {
            auto cmp = [&](int a, int b) {
                if (rank[static_cast<std::size_t>(a)] != rank[static_cast<std::size_t>(b)]) {
                    return rank[static_cast<std::size_t>(a)] < rank[static_cast<std::size_t>(b)];
                }
                const int ra = (a + k < n) ? rank[static_cast<std::size_t>(a + k)] : -1;
                const int rb = (b + k < n) ? rank[static_cast<std::size_t>(b + k)] : -1;
                return ra < rb;
            };
            std::sort(m_sa.begin(), m_sa.end(), cmp);
            tmp[static_cast<std::size_t>(m_sa[0])] = 0;
            for (int i = 1; i < n; ++i) {
                tmp[static_cast<std::size_t>(m_sa[static_cast<std::size_t>(i)])] =
                    tmp[static_cast<std::size_t>(m_sa[static_cast<std::size_t>(i - 1)])] +
                    (cmp(m_sa[static_cast<std::size_t>(i - 1)], m_sa[static_cast<std::size_t>(i)]) ? 1 : 0);
            }
            rank = tmp;
            if (rank[static_cast<std::size_t>(m_sa[static_cast<std::size_t>(n - 1)])] == n - 1) {
                break; // all suffixes distinct -> fully sorted
            }
        }
        buildLcp(rank);
    }

    const std::vector<int>& suffixArray() const { return m_sa; }
    const std::vector<int>& lcpArray() const { return m_lcp; }
    std::size_t size() const { return m_sa.size(); }

    // Does `pattern` occur in the text? (Empty pattern matches.)
    bool contains(std::string_view pattern) const {
        if (pattern.empty()) {
            return true;
        }
        return lowerBound(pattern) < static_cast<int>(m_sa.size()) &&
               suffixStartsWith(m_sa[static_cast<std::size_t>(lowerBound(pattern))], pattern);
    }

    // Number of (possibly overlapping) occurrences of `pattern`.
    int count(std::string_view pattern) const {
        if (pattern.empty()) {
            return static_cast<int>(m_text.size()) + 1;
        }
        const int lo = lowerBound(pattern);
        const int hi = upperBound(pattern);
        return hi - lo;
    }

    // Length of the longest substring that appears at least twice (0 if none). This is max(LCP).
    int longestRepeatedLength() const {
        int best = 0;
        for (int v : m_lcp) {
            best = std::max(best, v);
        }
        return best;
    }

private:
    void buildLcp(const std::vector<int>& rank) {
        const int n = static_cast<int>(m_text.size());
        int h = 0;
        for (int i = 0; i < n; ++i) {
            const int r = rank[static_cast<std::size_t>(i)];
            if (r > 0) {
                const int j = m_sa[static_cast<std::size_t>(r - 1)];
                while (i + h < n && j + h < n &&
                       m_text[static_cast<std::size_t>(i + h)] == m_text[static_cast<std::size_t>(j + h)]) {
                    ++h;
                }
                m_lcp[static_cast<std::size_t>(r)] = h;
                if (h > 0) {
                    --h;
                }
            } else {
                h = 0;
            }
        }
        m_lcp[0] = 0;
    }

    bool suffixStartsWith(int start, std::string_view pattern) const {
        const int n = static_cast<int>(m_text.size());
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            const int p = start + static_cast<int>(i);
            if (p >= n || m_text[static_cast<std::size_t>(p)] != pattern[i]) {
                return false;
            }
        }
        return true;
    }

    // Compare the suffix at `start` against `pattern`: <0 suffix smaller, 0 pattern is a prefix, >0 larger.
    int cmpSuffix(int start, std::string_view pattern) const {
        const int n = static_cast<int>(m_text.size());
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            const int p = start + static_cast<int>(i);
            if (p >= n) {
                return -1; // suffix ran out -> smaller
            }
            const unsigned char a = static_cast<unsigned char>(m_text[static_cast<std::size_t>(p)]);
            const unsigned char b = static_cast<unsigned char>(pattern[i]);
            if (a != b) {
                return a < b ? -1 : 1;
            }
        }
        return 0; // pattern is a prefix of the suffix
    }

    // First index in the SA whose suffix is >= pattern (and starts with it if present).
    int lowerBound(std::string_view pattern) const {
        int lo = 0, hi = static_cast<int>(m_sa.size());
        while (lo < hi) {
            const int mid = lo + (hi - lo) / 2;
            if (cmpSuffix(m_sa[static_cast<std::size_t>(mid)], pattern) < 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo;
    }

    // First index in the SA whose suffix does NOT start with pattern (past the match block).
    int upperBound(std::string_view pattern) const {
        int lo = 0, hi = static_cast<int>(m_sa.size());
        while (lo < hi) {
            const int mid = lo + (hi - lo) / 2;
            if (cmpSuffix(m_sa[static_cast<std::size_t>(mid)], pattern) <= 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo;
    }

    std::string m_text;
    std::vector<int> m_sa;
    std::vector<int> m_lcp;
};

} // namespace maz::core
