#pragma once

#include <cstddef>
#include <functional>
#include <vector>

// maz::core::SegmentTree — dynamic range queries with point updates for any associative operation.
// This fills the gap between the engine's two existing range structures: FenwickTree does prefix SUMS with
// point updates but cannot answer a range MIN/MAX; SparseTable answers range min/max in O(1) but only over a
// STATIC array that never changes. A segment tree does both — arbitrary range min / max / sum / gcd AND
// point updates, each in O(log n). The tool for a live heightfield's "tallest point in this span" that
// changes as terrain deforms, a scrolling audio meter's running peak, damage-over-segments, or any
// "combine over [l,r] while values keep changing" query. Templated on the combine op (default sum) with a
// caller-supplied identity, so min/max/gcd all work. Iterative (cache-friendly, no recursion); the query
// keeps left/right accumulators separate so it stays correct even for non-commutative operations.
// Header-only, std-only. Godot exposes no segment tree.
namespace maz::core {

template <typename T, typename Op = std::plus<T>>
class SegmentTree {
public:
    // Build a tree of `n` elements all equal to `identity`.
    SegmentTree(std::size_t n, T identity, Op op = Op())
        : m_n(n), m_id(identity), m_op(op), m_tree(n == 0 ? 0 : 2 * n, identity) {}

    // Build from initial values. `identity` is the neutral element for `op` (0 for sum, +inf for min, ...).
    SegmentTree(const std::vector<T>& init, T identity, Op op = Op())
        : m_n(init.size()),
          m_id(identity),
          m_op(op),
          m_tree(init.empty() ? 0 : 2 * init.size(), identity) {
        if (m_n == 0) {
            return;
        }
        for (std::size_t i = 0; i < m_n; ++i) {
            m_tree[m_n + i] = init[i];
        }
        for (std::size_t i = m_n - 1; i >= 1; --i) {
            m_tree[i] = m_op(m_tree[2 * i], m_tree[2 * i + 1]);
            if (i == 1) {
                break;
            }
        }
    }

    std::size_t size() const { return m_n; }

    // Read the current value at index i.
    T get(std::size_t i) const { return m_tree[m_n + i]; }

    // Assign `value` at index i and refresh the O(log n) path to the root.
    void update(std::size_t i, T value) {
        std::size_t pos = m_n + i;
        m_tree[pos] = value;
        for (pos >>= 1; pos >= 1; pos >>= 1) {
            m_tree[pos] = m_op(m_tree[2 * pos], m_tree[2 * pos + 1]);
            if (pos == 1) {
                break;
            }
        }
    }

    // Combine over the inclusive range [l, r]. Returns identity if the range is empty/invalid.
    T query(std::size_t l, std::size_t r) const {
        if (m_n == 0 || l >= m_n || l > r) {
            return m_id;
        }
        if (r >= m_n) {
            r = m_n - 1; // clamp an over-long upper bound
        }
        T resL = m_id;
        T resR = m_id;
        std::size_t lo = l + m_n;
        std::size_t hi = r + m_n + 1; // half-open upper bound
        for (; lo < hi; lo >>= 1, hi >>= 1) {
            if (lo & 1u) {
                resL = m_op(resL, m_tree[lo++]);
            }
            if (hi & 1u) {
                resR = m_op(m_tree[--hi], resR);
            }
        }
        return m_op(resL, resR);
    }

    // Combine over the whole array.
    T queryAll() const { return m_n == 0 ? m_id : m_tree[1]; }

private:
    std::size_t m_n;
    T m_id;
    Op m_op;
    std::vector<T> m_tree; // 1-based implicit tree of length 2n; leaves live at [n, 2n)
};

} // namespace maz::core
