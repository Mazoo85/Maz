#pragma once

#include <cstddef>
#include <iterator>
#include <set>
#include <vector>

// maz::core::RunningMedian — a fixed-window streaming MEDIAN filter: feed samples one at a time and read the
// median of the most recent N at any moment. The median is the great outlier-resistant smoother: a lone
// spike (a glitched sensor reading, a dropped-frame time, a network hiccup) is an extreme value that the
// median simply steps over, whereas a moving AVERAGE (core::MovingAverage) gets dragged toward the spike and
// smears it across the window. Crucially the median also preserves genuine step changes (edges) that a mean
// rounds off. This is the 1D streaming cousin of the engine's image median filter (render::medianFilter,
// which denoises a 2D image): use it for a jitter-free frame-time readout, a de-glitched analog stick or
// gyro axis, or robust smoothing of any noisy per-frame signal. Backed by an ordered multiset for O(log N)
// updates. Header-only, std-only, deterministic. Distinct from P2Quantile (a streaming ESTIMATE over all
// history) — this is the EXACT median of a sliding window. Godot ships no running-median filter.
namespace maz::core {

class RunningMedian {
public:
    explicit RunningMedian(std::size_t window)
        : m_window(window == 0 ? 1 : window), m_ring(window == 0 ? 1 : window, m_set.end()) {}

    // Add one sample, evicting the oldest once the window is full.
    void push(double x) {
        if (m_count == m_window) {
            m_set.erase(m_ring[m_head]); // drop the oldest value
        } else {
            ++m_count;
        }
        m_ring[m_head] = m_set.insert(x);
        m_head = (m_head + 1) % m_window;
    }

    // Median of the samples currently in the window (0 when empty). Even counts average the two middles.
    double median() const {
        if (m_count == 0) {
            return 0.0;
        }
        auto mid = m_set.begin();
        std::advance(mid, static_cast<std::ptrdiff_t>(m_count / 2));
        if (m_count % 2 == 1) {
            return *mid;
        }
        auto lo = mid;
        --lo;
        return (*lo + *mid) * 0.5;
    }

    std::size_t count() const { return m_count; }
    std::size_t capacity() const { return m_window; }
    bool full() const { return m_count == m_window; }

    void clear() {
        m_set.clear();
        for (auto& it : m_ring) {
            it = m_set.end();
        }
        m_head = 0;
        m_count = 0;
    }

private:
    std::size_t m_window;
    std::multiset<double> m_set;                              // current window contents, ordered
    std::vector<std::multiset<double>::iterator> m_ring;      // insertion order -> set iterator (for evict)
    std::size_t m_head = 0;
    std::size_t m_count = 0;
};

} // namespace maz::core
