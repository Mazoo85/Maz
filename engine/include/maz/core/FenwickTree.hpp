#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::core::FenwickTree — a Binary Indexed Tree: a mutable integer array that answers "sum of the
// first i elements" (and any range sum) in O(log n) while still allowing O(log n) updates to any
// element. A plain array gives O(1) update but O(n) prefix sums; a precomputed prefix array gives O(1)
// sums but O(n) update — the Fenwick tree is the standard structure that makes BOTH cheap at once. The
// key game use is DYNAMIC weighted random selection: findByPrefix() picks a bucket in O(log n) with
// probability proportional to its weight, and unlike a build-once alias table the weights can be
// changed between draws (loot tables that shift with luck stats, spawn tables that deplete as a wave
// clears, cumulative-frequency sampling). Also handy for running range sums over a mutable series —
// scoreboards, histograms, order statistics. 0-indexed API, header-only, std-only. Godot has no
// Fenwick tree.
namespace maz::core {

class FenwickTree {
public:
    FenwickTree() = default;
    explicit FenwickTree(std::size_t n) { reset(n); }

    // (Re)initialise to n zeroed elements.
    void reset(std::size_t n) {
        m_tree.assign(n + 1, 0); // 1-indexed internally; index 0 is unused
        m_n = n;
    }

    std::size_t size() const { return m_n; }

    // Add delta to element i.
    void add(std::size_t i, std::int64_t delta) {
        for (std::size_t x = i + 1; x <= m_n; x += lowbit(x)) {
            m_tree[x] += delta;
        }
    }

    // Sum of elements [0 .. i] inclusive.
    std::int64_t prefixSum(std::size_t i) const {
        std::int64_t s = 0;
        for (std::size_t x = i + 1; x > 0; x -= lowbit(x)) {
            s += m_tree[x];
        }
        return s;
    }

    // Sum of elements [l .. r] inclusive (l <= r).
    std::int64_t rangeSum(std::size_t l, std::size_t r) const {
        std::int64_t s = prefixSum(r);
        if (l > 0) {
            s -= prefixSum(l - 1);
        }
        return s;
    }

    // Sum of all elements.
    std::int64_t total() const { return m_n == 0 ? 0 : prefixSum(m_n - 1); }

    // Current value of element i.
    std::int64_t at(std::size_t i) const { return i == 0 ? prefixSum(0) : rangeSum(i, i); }

    // Overwrite element i with value.
    void set(std::size_t i, std::int64_t value) { add(i, value - at(i)); }

    // Smallest index i whose prefixSum(i) >= target (target >= 1). Returns size() if the total is
    // below target. For weighted sampling: draw target uniformly in [1, total()] and this returns the
    // chosen bucket with probability proportional to its weight (zero-weight buckets are never hit).
    std::size_t findByPrefix(std::int64_t target) const {
        if (target <= 0) {
            return 0;
        }
        std::size_t pos = 0;          // number of leading elements whose cumulative sum < target
        std::int64_t remaining = target;
        std::size_t step = 1;
        while ((step << 1) <= m_n) {
            step <<= 1;
        }
        for (; step > 0; step >>= 1) {
            if (pos + step <= m_n && m_tree[pos + step] < remaining) {
                pos += step;
                remaining -= m_tree[pos];
            }
        }
        return pos; // 0-indexed bucket (1-indexed pos+1)
    }

private:
    static std::size_t lowbit(std::size_t x) { return x & (~x + 1); } // isolate lowest set bit (x & -x)

    std::vector<std::int64_t> m_tree; // 1-indexed Fenwick array
    std::size_t m_n = 0;
};

} // namespace maz::core
