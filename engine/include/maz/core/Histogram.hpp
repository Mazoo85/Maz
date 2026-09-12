#pragma once

#include <cstddef>
#include <vector>

// maz::core::Histogram — fixed-range, equal-width histogram accumulator. Give it a value range
// [min, max) and a bin count; feed samples with add(). It keeps only the per-bin tallies (O(bins)
// memory, nothing per-sample), yet answers the questions a plain mean/variance can't: which value
// range is most common (mode), what fraction fell in a bin (frequency), and interpolated
// percentiles / median of the distribution's SHAPE. The distribution-shape complement to
// RunningStats (which gives mean/variance/min/max but no median or percentiles). Out-of-range
// samples are clamped into the edge bins so the tallies always sum to the total, while separate
// below()/above() counters report how many spilled past each end. The natural tool for a
// frame-time / latency distribution readout ("95th-percentile frame time"), telemetry buckets,
// damage/score spread analysis, and difficulty-tuning signals. Header-only, std-only. Godot has
// no histogram type.
namespace maz::core {

class Histogram {
public:
    // Build a histogram over [minValue, maxValue) split into binCount equal-width bins. A
    // non-positive range is nudged to a unit span and a zero bin count to a single bin, so the
    // object is always usable (no divide-by-zero, no empty bin vector).
    Histogram(double minValue, double maxValue, std::size_t binCount)
        : m_min(minValue),
          m_max(maxValue > minValue ? maxValue : minValue + 1.0),
          m_bins(binCount == 0 ? 1 : binCount, 0),
          m_width((m_max - m_min) / static_cast<double>(m_bins.size())) {}

    // Add one sample. Out-of-range values land in the nearest edge bin (and bump below()/above()).
    void add(double x) {
        ++m_total;
        if (x < m_min) {
            ++m_below;
        } else if (x >= m_max) {
            ++m_above;
        }
        ++m_bins[indexOf(x)];
    }

    void clear() {
        for (std::size_t& c : m_bins) {
            c = 0;
        }
        m_total = 0;
        m_below = 0;
        m_above = 0;
    }

    std::size_t binCount() const { return m_bins.size(); }
    std::size_t total() const { return m_total; }
    bool empty() const { return m_total == 0; }
    std::size_t below() const { return m_below; } // samples that fell short of min
    std::size_t above() const { return m_above; } // samples that reached/exceeded max
    double rangeMin() const { return m_min; }
    double rangeMax() const { return m_max; }
    double binWidth() const { return m_width; }

    // Tally in bin i (0 if i is out of range).
    std::size_t bin(std::size_t i) const { return i < m_bins.size() ? m_bins[i] : 0; }
    // Fraction of all samples in bin i, in [0,1] (0 when no samples yet).
    double frequency(std::size_t i) const {
        return m_total == 0 || i >= m_bins.size()
                   ? 0.0
                   : static_cast<double>(m_bins[i]) / static_cast<double>(m_total);
    }

    double binLow(std::size_t i) const { return m_min + static_cast<double>(i) * m_width; }
    double binHigh(std::size_t i) const { return m_min + static_cast<double>(i + 1) * m_width; }
    double binCenter(std::size_t i) const { return m_min + (static_cast<double>(i) + 0.5) * m_width; }

    // Index of the fullest bin (the mode bin); earliest on ties. 0 when empty.
    std::size_t modeBin() const {
        std::size_t best = 0;
        for (std::size_t i = 1; i < m_bins.size(); ++i) {
            if (m_bins[i] > m_bins[best]) {
                best = i;
            }
        }
        return best;
    }
    // Center value of the most common bin.
    double mode() const { return binCenter(modeBin()); }

    // Interpolated percentile: the value below which fraction p (in [0,1]) of the samples fall,
    // estimated by linear interpolation inside the bin that straddles the p-th sample. Clamped to
    // the range ends. Returns min when empty.
    double percentile(double p) const {
        if (m_total == 0 || p <= 0.0) {
            return m_min;
        }
        if (p >= 1.0) {
            return m_max;
        }
        const double target = p * static_cast<double>(m_total);
        std::size_t cum = 0;
        for (std::size_t i = 0; i < m_bins.size(); ++i) {
            const std::size_t next = cum + m_bins[i];
            if (m_bins[i] > 0 && static_cast<double>(next) >= target) {
                const double into =
                    (target - static_cast<double>(cum)) / static_cast<double>(m_bins[i]);
                return binLow(i) + into * m_width;
            }
            cum = next;
        }
        return m_max;
    }
    double median() const { return percentile(0.5); }

private:
    std::size_t indexOf(double x) const {
        if (x <= m_min) {
            return 0;
        }
        if (x >= m_max) {
            return m_bins.size() - 1;
        }
        // (x - m_min) is > 0 here, so the truncating cast is a true floor (no <cmath> needed).
        std::size_t idx = static_cast<std::size_t>((x - m_min) / m_width);
        if (idx >= m_bins.size()) {
            idx = m_bins.size() - 1;
        }
        return idx;
    }

    double m_min;
    double m_max;
    std::vector<std::size_t> m_bins;
    double m_width;
    std::size_t m_total = 0;
    std::size_t m_below = 0;
    std::size_t m_above = 0;
};

} // namespace maz::core
