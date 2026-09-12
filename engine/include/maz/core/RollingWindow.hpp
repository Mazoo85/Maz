#pragma once

#include <cstddef>
#include <deque>
#include <vector>

// maz::core::RollingWindow — fixed-capacity sliding-window statistics. Keeps only the most recent N
// samples and reports their sum / mean / min / max, dropping the oldest sample as each new one
// arrives. Unlike RunningStats (all-time, never forgets) this answers "what have the last N samples
// been doing" — the natural shape for a live "average FPS over the last 60 frames" readout, a
// recent-input smoother, a scrolling telemetry graph, or a short-horizon trend signal. The running
// sum makes mean() O(1); the min() / max() use monotonic index deques so they stay O(1) amortised
// even as the window slides (no rescan of the window on eviction). Header-only, std-only. Godot has
// no rolling-window accumulator.
namespace maz::core {

class RollingWindow {
public:
    // Window holding the last `capacity` samples (coerced to at least 1).
    explicit RollingWindow(std::size_t capacity)
        : m_ring(capacity == 0 ? 1 : capacity, 0.0) {}

    // Add one sample, evicting the oldest once the window is full.
    void push(double x) {
        const std::size_t cap = m_ring.size();
        const std::size_t idx = m_pushed;           // this sample's global index
        const std::size_t slot = idx % cap;

        if (m_pushed >= cap) {
            m_sum -= m_ring[slot];                   // the sample being overwritten leaves the window
        }
        m_ring[slot] = x;
        m_sum += x;
        ++m_pushed;

        // Monotonic min deque: indices with non-decreasing values, front = window minimum.
        while (!m_minDq.empty() && m_ring[m_minDq.back() % cap] >= x) {
            m_minDq.pop_back();
        }
        m_minDq.push_back(idx);
        // Monotonic max deque: indices with non-increasing values, front = window maximum.
        while (!m_maxDq.empty() && m_ring[m_maxDq.back() % cap] <= x) {
            m_maxDq.pop_back();
        }
        m_maxDq.push_back(idx);

        // Drop deque fronts that have slid out of the window.
        const std::size_t windowStart = m_pushed - size();
        while (!m_minDq.empty() && m_minDq.front() < windowStart) {
            m_minDq.pop_front();
        }
        while (!m_maxDq.empty() && m_maxDq.front() < windowStart) {
            m_maxDq.pop_front();
        }
    }

    void clear() {
        m_pushed = 0;
        m_sum = 0.0;
        m_minDq.clear();
        m_maxDq.clear();
    }

    std::size_t capacity() const { return m_ring.size(); }
    // Number of samples currently in the window (up to capacity).
    std::size_t size() const { return m_pushed < m_ring.size() ? m_pushed : m_ring.size(); }
    bool empty() const { return m_pushed == 0; }
    bool full() const { return m_pushed >= m_ring.size(); }

    double sum() const { return m_sum; }
    double mean() const { return m_pushed == 0 ? 0.0 : m_sum / static_cast<double>(size()); }
    double min() const {
        return m_minDq.empty() ? 0.0 : m_ring[m_minDq.front() % m_ring.size()];
    }
    double max() const {
        return m_maxDq.empty() ? 0.0 : m_ring[m_maxDq.front() % m_ring.size()];
    }
    // Most recently pushed sample (0.0 when empty).
    double latest() const {
        return m_pushed == 0 ? 0.0 : m_ring[(m_pushed - 1) % m_ring.size()];
    }

private:
    std::vector<double> m_ring;    // last `capacity` samples, indexed by globalIndex % capacity
    std::deque<std::size_t> m_minDq; // monotonic (increasing values) window-min index deque
    std::deque<std::size_t> m_maxDq; // monotonic (decreasing values) window-max index deque
    std::size_t m_pushed = 0;      // total pushes ever (window covers [m_pushed - size(), m_pushed))
    double m_sum = 0.0;
};

} // namespace maz::core
