#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "maz/core/StringUtils.hpp" // reuse the existing levenshtein() + similarity() (M290)

// maz::core fuzzy subsequence matching — the ranking primitive a command palette / search box needs,
// built on top of the edit-distance and bigram-similarity helpers StringUtils already provides (M290).
// Adds two things those don't: longestCommonSubsequenceLength() (the length of the longest in-order,
// not-necessarily-contiguous shared run — a diff/overlap measure distinct from edit distance), and
// fuzzyMatch() — an fzf/Sublime-style scorer that reports whether a short pattern's characters appear
// in order inside a candidate, where they landed, and how good the match is, rewarding runs of
// consecutive characters and matches on word boundaries (string start, after a separator, or a
// camelCase hump). Together with the Trie (prefix autocomplete) this ranks loose, abbreviation-style
// queries like "gp" -> "getPlayer". Godot's String offers only a bigram similarity ratio. Header-only,
// std-only, deterministic.
namespace maz::core {

namespace detail {
inline char fuzzyLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool fuzzySeparator(char c) {
    return c == ' ' || c == '_' || c == '-' || c == '/' || c == '.' || c == ':' || c == '\\';
}
// A boundary is the string start, a char after a separator, or a camelCase hump (lower->Upper).
inline bool fuzzyBoundary(const std::string& text, std::size_t i) {
    if (i == 0) {
        return true;
    }
    const char prev = text[i - 1];
    const char cur = text[i];
    if (fuzzySeparator(prev)) {
        return true;
    }
    const bool prevLower = prev >= 'a' && prev <= 'z';
    const bool curUpper = cur >= 'A' && cur <= 'Z';
    return prevLower && curUpper;
}
} // namespace detail

// Length of the longest subsequence common to both strings (not necessarily contiguous).
inline std::size_t longestCommonSubsequenceLength(const std::string& a, const std::string& b) {
    const std::size_t m = a.size();
    const std::size_t n = b.size();
    if (m == 0 || n == 0) {
        return 0;
    }
    std::vector<std::size_t> prev(n + 1, 0);
    std::vector<std::size_t> cur(n + 1, 0);
    for (std::size_t i = 1; i <= m; ++i) {
        for (std::size_t j = 1; j <= n; ++j) {
            cur[j] = (a[i - 1] == b[j - 1]) ? prev[j - 1] + 1 : std::max(prev[j], cur[j - 1]);
        }
        std::swap(prev, cur);
    }
    return prev[n];
}

struct FuzzyResult {
    bool matched = false;               // do all pattern chars appear in order?
    int score = 0;                      // higher = tighter match (0 when unmatched)
    std::vector<std::size_t> positions; // indices in text where pattern chars landed
};

// Does `pattern` fuzzily match `text` (leftmost in-order subsequence) and how good is it? An empty
// pattern matches anything with score 0. Case-insensitive unless caseSensitive is set.
inline FuzzyResult fuzzyMatch(const std::string& pattern, const std::string& text,
                              bool caseSensitive = false) {
    FuzzyResult r;
    if (pattern.empty()) {
        r.matched = true;
        return r;
    }
    std::size_t pi = 0;
    int prevIdx = -1;
    for (std::size_t ti = 0; ti < text.size() && pi < pattern.size(); ++ti) {
        char pc = pattern[pi];
        char tc = text[ti];
        if (!caseSensitive) {
            pc = detail::fuzzyLower(pc);
            tc = detail::fuzzyLower(tc);
        }
        if (pc == tc) {
            r.positions.push_back(ti);
            int s = 1;                                   // base per matched char
            if (detail::fuzzyBoundary(text, ti)) {
                s += 2;                                  // landed on a word boundary
            }
            if (prevIdx >= 0 && static_cast<std::size_t>(prevIdx) == ti - 1) {
                s += 3;                                  // adjacent to the previous match (a run)
            }
            r.score += s;
            prevIdx = static_cast<int>(ti);
            ++pi;
        }
    }
    r.matched = (pi == pattern.size());
    if (!r.matched) {
        r.score = 0;
        r.positions.clear();
    }
    return r;
}

} // namespace maz::core
