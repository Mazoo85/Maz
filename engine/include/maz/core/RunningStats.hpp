#pragma once

#include <cstddef>
#include <limits>

// maz::core::RunningStats — online (streaming) statistics via Welford's algorithm. Feed samples one at a
// time and read back count / mean / variance / standard deviation / min / max at any moment, in O(1)
// memory and a single pass, without storing the samples. Welford's recurrence is numerically stable
// (no catastrophic cancellation from the naive "sum of squares minus square of sum"), so it stays
// accurate even over millions of samples. The natural tool for live frame-time / FPS statistics, sensor
// and input smoothing diagnostics, telemetry aggregates, and adaptive-difficulty signals. Header-only,
// std-only. Godot has no running-statistics accumulator.
namespace maz::core {

class RunningStats {
public:
    RunningStats() = default;

    // Add one sample (Welford update for a stable running mean and variance).
    void push(double x) {
        ++m_count;
        const double delta = x - m_mean;
        m_mean += delta / static_cast<double>(m_count);
        const double delta2 = x - m_mean;
        m_m2 += delta * delta2;
        if (x < m_min) {
            m_min = x;
        }
        if (x > m_max) {
            m_max = x;
        }
        m_sum += x;
    }

    void clear() {
        m_count = 0;
        m_mean = 0.0;
        m_m2 = 0.0;
        m_sum = 0.0;
        m_min = std::numeric_limits<double>::infinity();
        m_max = -std::numeric_limits<double>::infinity();
    }

    std::size_t count() const { return m_count; }
    bool empty() const { return m_count == 0; }
    double sum() const { return m_sum; }
    double mean() const { return m_count == 0 ? 0.0 : m_mean; }
    double min() const { return m_count == 0 ? 0.0 : m_min; }
    double max() const { return m_count == 0 ? 0.0 : m_max; }

    // Population variance (divide by N): the spread of the samples you have. 0 for < 1 sample.
    double variance() const { return m_count == 0 ? 0.0 : m_m2 / static_cast<double>(m_count); }
    // Sample variance (divide by N-1): unbiased estimate of a wider population's variance. Needs >= 2.
    double sampleVariance() const {
        return m_count < 2 ? 0.0 : m_m2 / static_cast<double>(m_count - 1);
    }
    double stddev() const { return sqrtNonNeg(variance()); }
    double sampleStddev() const { return sqrtNonNeg(sampleVariance()); }

private:
    static double sqrtNonNeg(double v) {
        if (v <= 0.0) {
            return 0.0;
        }
        // Newton's method (no <cmath> dependency; converges fast for the non-negative reals here).
        double g = v;
        for (int i = 0; i < 40; ++i) {
            g = 0.5 * (g + v / g);
        }
        return g;
    }

    std::size_t m_count = 0;
    double m_mean = 0.0;
    double m_m2 = 0.0; // sum of squared deviations from the running mean
    double m_sum = 0.0;
    double m_min = std::numeric_limits<double>::infinity();
    double m_max = -std::numeric_limits<double>::infinity();
};

} // namespace maz::core
