#pragma once

#include <algorithm>
#include <string>
#include <vector>

// maz::core Jaro & Jaro-Winkler string similarity — a normalised [0,1] closeness score tuned for SHORT
// strings and typos, where 1 is identical and 0 is nothing in common. It complements the fuzzy tools the
// engine already has: StringUtils/FuzzyMatch give Levenshtein edit distance and an fzf-style subsequence
// scorer, but Jaro-Winkler measures similarity differently — it counts characters that match within a
// sliding window, penalises transposed pairs, and (the "Winkler" part) BOOSTS strings that share a leading
// prefix. That makes it the go-to metric for "did you mean...?" command/name suggestions, matching a typed
// player or item name against a list, and de-duplicating near-identical strings, where a shared start
// matters and small transpositions ("MARTHA" vs "MARHTA") should barely count. Case-sensitive on raw bytes
// (lower-case the inputs first for case-insensitive matching). Header-only, std-only, deterministic.
namespace maz::core {

// Jaro similarity in [0,1]. Two empty strings score 1; one empty scores 0.
inline double jaro(const std::string& a, const std::string& b) {
    const int la = static_cast<int>(a.size());
    const int lb = static_cast<int>(b.size());
    if (la == 0 && lb == 0) {
        return 1.0;
    }
    if (la == 0 || lb == 0) {
        return 0.0;
    }

    // Two chars match only if they are within this window of each other's index.
    const int window = std::max(0, std::max(la, lb) / 2 - 1);

    std::vector<char> aMatched(static_cast<std::size_t>(la), 0);
    std::vector<char> bMatched(static_cast<std::size_t>(lb), 0);

    int matches = 0;
    for (int i = 0; i < la; ++i) {
        const int lo = std::max(0, i - window);
        const int hi = std::min(lb - 1, i + window);
        for (int j = lo; j <= hi; ++j) {
            if (!bMatched[static_cast<std::size_t>(j)] &&
                a[static_cast<std::size_t>(i)] == b[static_cast<std::size_t>(j)]) {
                aMatched[static_cast<std::size_t>(i)] = 1;
                bMatched[static_cast<std::size_t>(j)] = 1;
                ++matches;
                break;
            }
        }
    }
    if (matches == 0) {
        return 0.0;
    }

    // Count transpositions: matched chars taken in order that don't line up.
    int transpositions = 0;
    int k = 0;
    for (int i = 0; i < la; ++i) {
        if (!aMatched[static_cast<std::size_t>(i)]) {
            continue;
        }
        while (!bMatched[static_cast<std::size_t>(k)]) {
            ++k;
        }
        if (a[static_cast<std::size_t>(i)] != b[static_cast<std::size_t>(k)]) {
            ++transpositions;
        }
        ++k;
    }
    const double m = static_cast<double>(matches);
    const double t = static_cast<double>(transpositions) / 2.0;
    return (m / static_cast<double>(la) + m / static_cast<double>(lb) + (m - t) / m) / 3.0;
}

// Jaro-Winkler similarity in [0,1]: Jaro, boosted by a shared leading prefix (up to `maxPrefix` chars,
// scaled by `prefixScale`). The standard parameters are prefixScale = 0.1 and maxPrefix = 4.
inline double jaroWinkler(const std::string& a, const std::string& b, double prefixScale = 0.1,
                          int maxPrefix = 4) {
    const double j = jaro(a, b);
    const int limit = std::min({maxPrefix, static_cast<int>(a.size()), static_cast<int>(b.size())});
    int prefix = 0;
    for (int i = 0; i < limit; ++i) {
        if (a[static_cast<std::size_t>(i)] == b[static_cast<std::size_t>(i)]) {
            ++prefix;
        } else {
            break;
        }
    }
    return j + static_cast<double>(prefix) * prefixScale * (1.0 - j);
}

} // namespace maz::core
