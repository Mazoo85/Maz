#pragma once

#include <cstddef>
#include <vector>

// maz::core::SummedAreaTable — a 2D prefix-sum table (a.k.a. integral image, Crow 1984) that answers the
// SUM (or AVERAGE) over ANY axis-aligned rectangle in O(1), no matter how large the rectangle, after an
// O(width*height) build. Each cell of the table stores the sum of everything above-and-left of it, so a
// rectangle sum is just four table lookups (bottom-right - top-strip - left-strip + double-counted-corner).
// This is the trick behind a constant-time box blur (any radius costs the same), average brightness or
// height over a region, adaptive/local thresholding, fast region queries on an influence or heat map, and
// Viola-Jones-style feature sums. It generalises the engine's 1D range structures (FenwickTree does 1D
// prefix sums with updates; SparseTable does 1D idempotent range queries) to two dimensions for a STATIC
// grid. Templated on the accumulator type (double for real grids, a wide integer for exact counts).
// Header-only, std-only, deterministic. Godot ships no summed-area table.
namespace maz::core {

template <typename T = double>
class SummedAreaTable {
public:
    // Build from a row-major width*height grid.
    SummedAreaTable(const std::vector<T>& values, int width, int height)
        : m_w(width < 0 ? 0 : width), m_h(height < 0 ? 0 : height) {
        if (m_w == 0 || m_h == 0 || values.size() != static_cast<std::size_t>(m_w) *
                                                          static_cast<std::size_t>(m_h)) {
            m_w = 0;
            m_h = 0;
            return;
        }
        // (w+1) x (h+1) table with a zero border so lookups never go out of range.
        const std::size_t sw = static_cast<std::size_t>(m_w) + 1;
        const std::size_t sh = static_cast<std::size_t>(m_h) + 1;
        m_sat.assign(sw * sh, T{});
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                const T v = values[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
                                   static_cast<std::size_t>(x)];
                m_sat[sat(x + 1, y + 1)] =
                    v + m_sat[sat(x, y + 1)] + m_sat[sat(x + 1, y)] - m_sat[sat(x, y)];
            }
        }
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool valid() const { return m_w > 0 && m_h > 0; }

    // Sum over the inclusive rectangle [x0,y0]..[x1,y1]. Coordinates are clamped to the grid and swapped if
    // given out of order; an empty/invalid table returns 0.
    T rectSum(int x0, int y0, int x1, int y1) const {
        if (m_w == 0 || m_h == 0) {
            return T{};
        }
        if (x1 < x0) { const int t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { const int t = y0; y0 = y1; y1 = t; }
        x0 = clampX(x0);
        x1 = clampX(x1);
        y0 = clampY(y0);
        y1 = clampY(y1);
        // Four corners of the inclusive rect map to exclusive table indices.
        return m_sat[sat(x1 + 1, y1 + 1)] - m_sat[sat(x0, y1 + 1)] - m_sat[sat(x1 + 1, y0)] +
               m_sat[sat(x0, y0)];
    }

    // Average over the inclusive rectangle (0 for an empty/invalid table).
    double rectMean(int x0, int y0, int x1, int y1) const {
        if (m_w == 0 || m_h == 0) {
            return 0.0;
        }
        if (x1 < x0) { const int t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { const int t = y0; y0 = y1; y1 = t; }
        const int cx0 = clampX(x0), cx1 = clampX(x1), cy0 = clampY(y0), cy1 = clampY(y1);
        const long long count = static_cast<long long>(cx1 - cx0 + 1) *
                                static_cast<long long>(cy1 - cy0 + 1);
        return static_cast<double>(rectSum(cx0, cy0, cx1, cy1)) / static_cast<double>(count);
    }

    // Total sum over the whole grid.
    T total() const { return m_w == 0 ? T{} : rectSum(0, 0, m_w - 1, m_h - 1); }

private:
    std::size_t sat(int x, int y) const {
        return static_cast<std::size_t>(y) * (static_cast<std::size_t>(m_w) + 1) +
               static_cast<std::size_t>(x);
    }
    int clampX(int x) const { return x < 0 ? 0 : (x >= m_w ? m_w - 1 : x); }
    int clampY(int y) const { return y < 0 ? 0 : (y >= m_h ? m_h - 1 : y); }

    int m_w;
    int m_h;
    std::vector<T> m_sat;
};

} // namespace maz::core
