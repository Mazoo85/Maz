#pragma once

#include <cstddef>
#include <deque>
#include <utility>
#include <vector>

// maz::core::MovingAverage — fixed-window rolling statistics over the last N samples: mean in O(1) per
// push, plus the window MIN and MAX in O(1) amortized (monotonic deques). This is distinct from the
// engine's other streaming stats: RunningStats (Welford) averages over ALL samples ever seen and can never
// "forget" old data; P2Quantile tracks a streaming percentile; RingBuffer is a raw ring with no reductions.
// A moving average deliberately forgets: it reports the mean of only the most recent N values, so it tracks
// a changing signal instead of drifting toward a lifetime average. The canonical tool for a smooth "N-frame
// average FPS" readout, denoising a jittery input axis or sensor, a rolling damage-per-second meter, or any
// "recent trend, not all-time" number. The windowed min/max come free for "worst frame time in the last
// second" style readouts. Header-only, std-only, deterministic. Godot ships no moving-average accumulator.
namespace maz::core {

template <typename T>
class MovingAverage {
public:
    // Window size N (>=1); clamped up to 1 if zero is passed.
    explicit MovingAverage(std::size_t window)
        : m_window(window == 0 ? 1 : window), m_ring(window == 0 ? 1 : window, T{}) {}

    // Add one sample, evicting the oldest once the window is full. O(1) amortized.
    void push(T x) {
        const std::size_t seq = m_pushCount;

        // Running sum over the ring: subtract the value being overwritten once full.
        const std::size_t idx = seq % m_window;
        if (m_count == m_window) {
            m_sum -= m_ring[idx];
        } else {
            ++m_count;
        }
        m_ring[idx] = x;
        m_sum += x;

        // Monotonic deque for the window minimum (values non-decreasing front->back).
        while (!m_minDq.empty() && m_minDq.back().second >= x) {
            m_minDq.pop_back();
        }
        m_minDq.emplace_back(seq, x);
        while (m_minDq.front().first + m_window <= seq) {
            m_minDq.pop_front();
        }

        // Monotonic deque for the window maximum (values non-increasing front->back).
        while (!m_maxDq.empty() && m_maxDq.back().second <= x) {
            m_maxDq.pop_back();
        }
        m_maxDq.emplace_back(seq, x);
        while (m_maxDq.front().first + m_window <= seq) {
            m_maxDq.pop_front();
        }

        ++m_pushCount;
    }

    // Mean of the samples currently in the window (0 when empty).
    double average() const {
        return m_count == 0 ? 0.0 : static_cast<double>(m_sum) / static_cast<double>(m_count);
    }

    // Sum of the samples currently in the window.
    T sum() const { return m_sum; }

    // Minimum / maximum over the current window (T{} when empty).
    T min() const { return m_minDq.empty() ? T{} : m_minDq.front().second; }
    T max() const { return m_maxDq.empty() ? T{} : m_maxDq.front().second; }

    std::size_t count() const { return m_count; }     // samples currently held (<= window)
    std::size_t capacity() const { return m_window; }  // the window size N
    bool full() const { return m_count == m_window; }

    void clear() {
        m_sum = T{};
        m_count = 0;
        m_pushCount = 0;
        m_minDq.clear();
        m_maxDq.clear();
    }

private:
    std::size_t m_window;
    std::vector<T> m_ring;                           // last N values, indexed by seq % window
    std::deque<std::pair<std::size_t, T>> m_minDq;   // (seq, value), monotonic non-decreasing
    std::deque<std::pair<std::size_t, T>> m_maxDq;   // (seq, value), monotonic non-increasing
    T m_sum{};
    std::size_t m_count = 0;      // number of valid samples in the window
    std::size_t m_pushCount = 0;  // total pushes ever (drives the sequence numbers)
};

} // namespace maz::core
