#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>

// maz::core::P2Quantile — the P-Square (P²) algorithm of Jain & Chlamtac (1985) for estimating a single
// quantile (median, p95, p99, …) of a data stream in CONSTANT memory and a single pass, WITHOUT storing
// the samples. RunningStats (Welford) gives you a streaming mean/variance but cannot answer "what is the
// 99th-percentile frame time?"; math::quantile answers it exactly but must hold the whole dataset in RAM;
// core::Histogram approximates it but needs you to pick bin edges up front. P² needs neither: it keeps
// just five running "markers" (order statistics), nudges their positions and heights as each sample
// arrives, and reads back a live estimate that provably converges to the true quantile. The natural tool
// for live latency/percentile telemetry — p95 network ping, p99 frame time, "how bad is the slow 1%?" —
// over an unbounded stream. Header-only, std-only, deterministic. Godot ships no streaming quantile.
namespace maz::core {

class P2Quantile {
public:
    // p in [0,1]: the quantile to track (0.5 = median, 0.95 = 95th percentile, …).
    explicit P2Quantile(double p) : m_p(p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p)) {}

    // Feed one observation. First five samples seed the markers; after that P² updates incrementally.
    void push(double x) {
        if (m_count < 5) {
            m_q[static_cast<std::size_t>(m_count)] = x;
            ++m_count;
            if (m_count == 5) {
                // Sort the first five (insertion sort — tiny, deterministic) to seed the markers.
                for (int i = 1; i < 5; ++i) {
                    const double key = m_q[static_cast<std::size_t>(i)];
                    int j = i - 1;
                    while (j >= 0 && m_q[static_cast<std::size_t>(j)] > key) {
                        m_q[static_cast<std::size_t>(j + 1)] = m_q[static_cast<std::size_t>(j)];
                        --j;
                    }
                    m_q[static_cast<std::size_t>(j + 1)] = key;
                }
                for (int i = 0; i < 5; ++i) {
                    m_n[static_cast<std::size_t>(i)] = i;                 // marker positions 0..4
                }
                m_np[0] = 0.0;
                m_np[1] = 2.0 * m_p;
                m_np[2] = 4.0 * m_p;
                m_np[3] = 2.0 + 2.0 * m_p;
                m_np[4] = 4.0;
                m_dn[0] = 0.0;
                m_dn[1] = m_p / 2.0;
                m_dn[2] = m_p;
                m_dn[3] = (1.0 + m_p) / 2.0;
                m_dn[4] = 1.0;
            }
            return;
        }

        // 1. Find the cell k that x falls into and update the running min/max endpoints.
        int k;
        if (x < m_q[0]) {
            m_q[0] = x;
            k = 0;
        } else if (x < m_q[1]) {
            k = 0;
        } else if (x < m_q[2]) {
            k = 1;
        } else if (x < m_q[3]) {
            k = 2;
        } else if (x <= m_q[4]) {
            k = 3;
        } else {
            m_q[4] = x;
            k = 3;
        }

        // 2. Increment positions of the markers to the right of the cell, and the desired positions.
        for (int i = k + 1; i < 5; ++i) {
            m_n[static_cast<std::size_t>(i)] += 1;
        }
        for (int i = 0; i < 5; ++i) {
            m_np[static_cast<std::size_t>(i)] += m_dn[static_cast<std::size_t>(i)];
        }

        // 3. Adjust the three interior markers if they drifted from their desired positions.
        for (int i = 1; i <= 3; ++i) {
            const std::size_t s = static_cast<std::size_t>(i);
            const double d = m_np[s] - static_cast<double>(m_n[s]);
            const int gapR = m_n[s + 1] - m_n[s]; // room to the right marker
            const int gapL = m_n[s] - m_n[s - 1]; // room to the left marker
            if ((d >= 1.0 && gapR > 1) || (d <= -1.0 && gapL > 1)) {
                const int dir = d >= 0.0 ? 1 : -1;
                const double parab = parabolic(i, dir);
                if (m_q[s - 1] < parab && parab < m_q[s + 1]) {
                    m_q[s] = parab;
                } else {
                    m_q[s] = linear(i, dir);
                }
                m_n[s] += dir;
            }
        }
        ++m_count;
    }

    // The current quantile estimate. Exact for <= 5 samples; the P² estimate thereafter.
    double value() const {
        if (m_count == 0) {
            return 0.0;
        }
        if (m_count < 5) {
            // Fewer than five samples: return the interpolated order statistic of what we have so far.
            // Copy/sort via std::array + std::sort (not hand-indexed loops, which some optimizers
            // mis-flag as out-of-range after inlining) — the count is always in [1,4] on this path.
            const std::size_t n = static_cast<std::size_t>(m_count);
            std::array<double, 5> tmp{};
            std::copy_n(std::begin(m_q), n, tmp.begin());
            std::sort(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(n));
            const double idx = m_p * static_cast<double>(n - 1);
            const std::size_t lo = static_cast<std::size_t>(idx);
            const std::size_t hi = lo + 1 < n ? lo + 1 : lo;
            const double frac = idx - static_cast<double>(lo);
            return tmp[lo] + frac * (tmp[hi] - tmp[lo]);
        }
        return m_q[2]; // the middle marker tracks the requested quantile
    }

    long long count() const { return m_count; }

    void clear() {
        m_count = 0;
        for (int i = 0; i < 5; ++i) {
            m_q[static_cast<std::size_t>(i)] = 0.0;
            m_n[static_cast<std::size_t>(i)] = 0;
            m_np[static_cast<std::size_t>(i)] = 0.0;
            m_dn[static_cast<std::size_t>(i)] = 0.0;
        }
    }

private:
    // Piecewise-parabolic prediction (P² eq.) of marker i moved by dir (+1/-1).
    double parabolic(int i, int dir) const {
        const std::size_t s = static_cast<std::size_t>(i);
        const double d = static_cast<double>(dir);
        const double nL = static_cast<double>(m_n[s - 1]);
        const double nC = static_cast<double>(m_n[s]);
        const double nR = static_cast<double>(m_n[s + 1]);
        const double qL = m_q[s - 1];
        const double qC = m_q[s];
        const double qR = m_q[s + 1];
        return qC + d / (nR - nL) *
                        ((nC - nL + d) * (qR - qC) / (nR - nC) +
                         (nR - nC - d) * (qC - qL) / (nC - nL));
    }

    // Linear fallback when the parabolic prediction would leave the neighbouring marker interval.
    double linear(int i, int dir) const {
        const std::size_t s = static_cast<std::size_t>(i);
        const std::size_t adj = static_cast<std::size_t>(i + dir);
        return m_q[s] + static_cast<double>(dir) * (m_q[adj] - m_q[s]) /
                            static_cast<double>(m_n[adj] - m_n[s]);
    }

    double m_p;
    int m_count = 0;
    double m_q[5] = {0, 0, 0, 0, 0};   // marker heights (the estimated order statistics)
    int m_n[5] = {0, 0, 0, 0, 0};      // marker positions (integer sample ranks)
    double m_np[5] = {0, 0, 0, 0, 0};  // desired marker positions
    double m_dn[5] = {0, 0, 0, 0, 0};  // desired-position increments per sample
};

} // namespace maz::core
