#pragma once

#include <cstddef>
#include <vector>

// maz::core sequence diff / longest common subsequence — compare two sequences and produce the minimal edit
// script (keeps, deletions, insertions) that turns one into the other, built on the classic longest-common-
// subsequence dynamic program. This is the engine behind: showing what changed between two versions of a
// save/scene/config, computing a compact patch to send over the network or store in an undo stack, merging or
// reconciling lists, and text/line diffing in tools. Templated on the element type (chars, lines, ids, any
// equality-comparable T). Godot has no diff utility. Header-only, std-only, deterministic. O(n*m) time/space
// (fine for the moderate sequences a game/tool diffs).
namespace maz::core {

enum class DiffOp { Keep, Delete, Insert }; // Keep = common to both; Delete = only in A; Insert = only in B

template <typename T>
struct DiffEntry {
    DiffOp op;
    T value;
};

namespace diff_detail {
// LCS-length table: lcs[i*(m+1)+j] = length of the longest common subsequence of a[0..i) and b[0..j).
template <typename T>
inline std::vector<int> lcsTable(const std::vector<T>& a, const std::vector<T>& b) {
    const std::size_t n = a.size(), m = b.size();
    std::vector<int> dp((n + 1) * (m + 1), 0);
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            if (a[i - 1] == b[j - 1]) {
                dp[i * (m + 1) + j] = dp[(i - 1) * (m + 1) + (j - 1)] + 1;
            } else {
                const int up = dp[(i - 1) * (m + 1) + j];
                const int left = dp[i * (m + 1) + (j - 1)];
                dp[i * (m + 1) + j] = up >= left ? up : left;
            }
        }
    }
    return dp;
}
} // namespace diff_detail

// The longest common subsequence of `a` and `b` (a sequence that is a subsequence of BOTH, of maximal length).
template <typename T>
inline std::vector<T> longestCommonSubsequence(const std::vector<T>& a, const std::vector<T>& b) {
    const std::size_t m = b.size();
    const std::vector<int> dp = diff_detail::lcsTable(a, b);
    std::vector<T> out;
    std::size_t i = a.size(), j = b.size();
    while (i > 0 && j > 0) {
        if (a[i - 1] == b[j - 1]) {
            out.push_back(a[i - 1]);
            --i;
            --j;
        } else if (dp[(i - 1) * (m + 1) + j] >= dp[i * (m + 1) + (j - 1)]) {
            --i;
        } else {
            --j;
        }
    }
    // Collected back-to-front; reverse into order.
    for (std::size_t k = 0; k < out.size() / 2; ++k) {
        const T tmp = out[k];
        out[k] = out[out.size() - 1 - k];
        out[out.size() - 1 - k] = tmp;
    }
    return out;
}

// The edit script turning `a` into `b`: an ordered list of Keep / Delete (from A) / Insert (from B) entries.
// Filtering to {Keep, Delete} and reading the values reproduces `a`; {Keep, Insert} reproduces `b`.
template <typename T>
inline std::vector<DiffEntry<T>> diff(const std::vector<T>& a, const std::vector<T>& b) {
    const std::size_t m = b.size();
    const std::vector<int> dp = diff_detail::lcsTable(a, b);
    std::vector<DiffEntry<T>> rev;
    std::size_t i = a.size(), j = b.size();
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i - 1] == b[j - 1]) {
            rev.push_back({DiffOp::Keep, a[i - 1]});
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i * (m + 1) + (j - 1)] >= dp[(i - 1) * (m + 1) + j])) {
            rev.push_back({DiffOp::Insert, b[j - 1]});
            --j;
        } else {
            rev.push_back({DiffOp::Delete, a[i - 1]});
            --i;
        }
    }
    std::vector<DiffEntry<T>> out;
    out.reserve(rev.size());
    for (std::size_t k = rev.size(); k-- > 0;) {
        out.push_back(rev[k]);
    }
    return out;
}

} // namespace maz::core
