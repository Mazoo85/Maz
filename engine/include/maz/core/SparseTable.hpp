#pragma once

#include <bit>
#include <cstddef>
#include <vector>

// maz::core::SparseTable — O(1) range queries over a STATIC array for any idempotent associative
// operation (min, max, gcd, bitwise and/or). After an O(n log n) build it answers "the combined value
// over [l, r]" in constant time by overlapping two power-of-two blocks. This complements FenwickTree
// (which does prefix sums with point updates): a sparse table can't be updated, but for a fixed array it
// answers min/max range queries far faster. The tool for "tallest terrain height in this span", static
// interval min/max, and range-minimum-query building blocks. Godot exposes no RMQ structure, so this is
// a beyond-Godot utility. Header-only, std-only.
namespace maz::core {

template <typename T>
struct MinOp {
    T operator()(const T& a, const T& b) const { return b < a ? b : a; }
};
template <typename T>
struct MaxOp {
    T operator()(const T& a, const T& b) const { return a < b ? b : a; }
};

// `Op` must be idempotent (op(x, x) == x) and associative — true for min/max/gcd/bitwise-and/or.
template <typename T, typename Op = MinOp<T>>
class SparseTable {
public:
    SparseTable() = default;
    explicit SparseTable(const std::vector<T>& data) { build(data); }

    void build(const std::vector<T>& data) {
        m_n = data.size();
        m_levels.clear();
        if (m_n == 0) {
            return;
        }
        const std::size_t k = static_cast<std::size_t>(std::bit_width(m_n)); // floor(log2 n)+1 levels
        m_levels.assign(k, std::vector<T>(m_n));
        m_levels[0] = data;
        for (std::size_t j = 1; j < k; ++j) {
            const std::size_t span = std::size_t{1} << j;
            const std::size_t half = span >> 1;
            for (std::size_t i = 0; i + span <= m_n; ++i) {
                m_levels[j][i] = m_op(m_levels[j - 1][i], m_levels[j - 1][i + half]);
            }
        }
    }

    bool empty() const { return m_n == 0; }
    std::size_t size() const { return m_n; }

    // Combined value over the inclusive range [l, r]. Requires 0 <= l <= r < size().
    T query(std::size_t l, std::size_t r) const {
        const std::size_t len = r - l + 1;
        const std::size_t j = static_cast<std::size_t>(std::bit_width(len)) - 1; // floor(log2 len)
        return m_op(m_levels[j][l], m_levels[j][r - (std::size_t{1} << j) + 1]);
    }

private:
    std::size_t m_n = 0;
    std::vector<std::vector<T>> m_levels;
    Op m_op{};
};

} // namespace maz::core
