#pragma once

// maz::math descriptive statistics — summarize a dataset of samples. Where core::RunningStats is a
// streaming single-pass estimator (mean/variance/min/max as values arrive) and core::Histogram gives
// binned, approximate percentiles of a distribution's shape, this operates on a full dataset in memory
// and computes EXACT order statistics (median, arbitrary quantiles by linear interpolation) plus the
// two-variable measures neither of those provides: covariance and Pearson correlation between a pair of
// datasets. It is the tool for analyzing playtest/telemetry samples ("median session length", "95th-
// percentile damage", "how strongly does accuracy correlate with score"), validating that a procedural
// generator's output matches a target distribution, or any offline batch summary. It pairs with
// LeastSquares (fit a trend) — correlation quantifies how linear that trend is. Accumulation is in
// double for conditioning. Godot exposes no statistics helpers to gameplay code, so this is a
// beyond-Godot numerics utility. Header-only, std-only, deterministic.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::math {

// Sum of all samples (0 for an empty set).
inline double sum(const std::vector<float>& v) {
    double s = 0.0;
    for (float x : v) s += static_cast<double>(x);
    return s;
}

// Arithmetic mean (0 for an empty set).
inline double mean(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    return sum(v) / static_cast<double>(v.size());
}

// Variance. sample=true uses the unbiased (n-1) divisor and needs >= 2 points; sample=false uses the
// population (n) divisor and needs >= 1. Returns 0 when there are too few points.
inline double variance(const std::vector<float>& v, bool sample = true) {
    const std::size_t n = v.size();
    if (n == 0 || (sample && n < 2)) return 0.0;
    const double m = mean(v);
    double acc = 0.0;
    for (float x : v) {
        const double d = static_cast<double>(x) - m;
        acc += d * d;
    }
    const double denom = sample ? static_cast<double>(n - 1) : static_cast<double>(n);
    return acc / denom;
}

// Standard deviation (square root of the variance).
inline double standardDeviation(const std::vector<float>& v, bool sample = true) {
    return std::sqrt(variance(v, sample));
}

// The q-quantile (q in [0,1], clamped) by linear interpolation between the two nearest ranks — the
// same "linear" method NumPy uses by default. q=0 -> min, q=1 -> max, q=0.5 -> median.
inline double quantile(std::vector<float> v, double q) {
    if (v.empty()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    std::sort(v.begin(), v.end());
    if (v.size() == 1) return static_cast<double>(v[0]);
    const double pos = q * static_cast<double>(v.size() - 1);
    const double lo = std::floor(pos);
    const double frac = pos - lo;
    const std::size_t i = static_cast<std::size_t>(lo);
    const double a = static_cast<double>(v[i]);
    if (i + 1 >= v.size()) return a;
    const double b = static_cast<double>(v[i + 1]);
    return a + frac * (b - a);
}

// Median: the middle value (average of the two middle values for an even count). Exact, from a sort.
inline double median(const std::vector<float>& v) {
    return quantile(v, 0.5);
}

// Smallest / largest sample (0 for an empty set).
inline double minValue(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    double m = static_cast<double>(v[0]);
    for (float x : v) m = std::min(m, static_cast<double>(x));
    return m;
}
inline double maxValue(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    double m = static_cast<double>(v[0]);
    for (float x : v) m = std::max(m, static_cast<double>(x));
    return m;
}

// Max minus min (0 for an empty set).
inline double range(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    return maxValue(v) - minValue(v);
}

// Covariance of paired samples x, y (must be the same length). sample=true uses the (n-1) divisor.
// Returns 0 on length mismatch or too few points. covariance(x, x) == variance(x).
inline double covariance(const std::vector<float>& x, const std::vector<float>& y, bool sample = true) {
    const std::size_t n = x.size();
    if (n != y.size() || n == 0 || (sample && n < 2)) return 0.0;
    const double mx = mean(x);
    const double my = mean(y);
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        acc += (static_cast<double>(x[i]) - mx) * (static_cast<double>(y[i]) - my);
    const double denom = sample ? static_cast<double>(n - 1) : static_cast<double>(n);
    return acc / denom;
}

// Pearson correlation coefficient of paired samples x, y in [-1, 1]. +1/-1 = perfectly linear, 0 = no
// linear relationship. Returns 0 on length mismatch, too few points, or a constant input (zero spread).
inline double correlation(const std::vector<float>& x, const std::vector<float>& y) {
    const std::size_t n = x.size();
    if (n != y.size() || n < 2) return 0.0;
    const double mx = mean(x);
    const double my = mean(y);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = static_cast<double>(x[i]) - mx;
        const double dy = static_cast<double>(y[i]) - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    const double denom = std::sqrt(sxx * syy);
    if (denom < 1e-12) return 0.0; // one variable is constant -> correlation undefined
    return std::clamp(sxy / denom, -1.0, 1.0);
}

} // namespace maz::math
