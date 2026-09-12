#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

// maz::core Damerau–Levenshtein (optimal string alignment) distance — like the plain edit distance
// (core::levenshtein) but counts a SWAP of two adjacent characters as a SINGLE edit. That matters because
// transposition is the single most common human typo ("teh"→"the", "recieve"→"receive"): Levenshtein charges
// it as two edits (delete + insert), Damerau charges one, so ranking search results / command-palette
// matches / player-name lookups by this distance tolerates typos the way people actually make them. This is
// the restricted (OSA) variant — adjacent transpositions only, no substring re-edited twice — which is
// symmetric and always ≤ the Levenshtein distance. Godot exposes no edit-distance utility. Header-only,
// std-only, deterministic.
namespace maz::core {

// Optimal-string-alignment (restricted Damerau–Levenshtein) edit distance between `a` and `b`: the minimum
// number of single-character insertions, deletions, substitutions and ADJACENT transpositions to turn one
// into the other.
inline std::size_t damerauLevenshtein(const std::string& a, const std::string& b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }
    const std::size_t w = m + 1;
    std::vector<std::size_t> d((n + 1) * w);
    auto at = [&](std::size_t i, std::size_t j) -> std::size_t& { return d[i * w + j]; };
    for (std::size_t i = 0; i <= n; ++i) {
        at(i, 0) = i;
    }
    for (std::size_t j = 0; j <= m; ++j) {
        at(0, j) = j;
    }
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0u : 1u;
            std::size_t best = std::min({at(i - 1, j) + 1,     // deletion
                                         at(i, j - 1) + 1,     // insertion
                                         at(i - 1, j - 1) + cost}); // substitution / match
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                best = std::min(best, at(i - 2, j - 2) + 1); // adjacent transposition
            }
            at(i, j) = best;
        }
    }
    return at(n, m);
}

} // namespace maz::core
