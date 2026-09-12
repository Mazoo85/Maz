#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::math::MonotoneCubic — piecewise cubic Hermite interpolation with the Fritsch-Carlson tangent
// correction (PCHIP): a smooth C1 curve through your data points that provably NEVER OVERSHOOTS. This is
// the crucial difference from the engine's other interpolators: a natural CubicSpline is C2 but can bulge
// past the data (a run of equal values can dip below or rise above them), and Catmull-Rom (VectorOps)
// overshoots too. PCHIP guarantees the curve stays monotone wherever the data is monotone and never leaves
// the bracket of its neighbouring samples — so it is the right tool for a tone / gamma / difficulty curve,
// a health or fuel gauge response, an audio envelope, or a terrain cross-section that must not dip below
// the sampled heights. Tangents are chosen from secant slopes then clamped so no segment can overshoot.
// Build once from strictly-increasing x with matching y; eval() clamps to the endpoints outside the range.
// O(n) build, O(log n) eval. Header-only, std-only, deterministic. Godot has no monotone interpolant.
namespace maz::math {

class MonotoneCubic {
public:
    MonotoneCubic() = default;
    MonotoneCubic(const std::vector<double>& xs, const std::vector<double>& ys) { build(xs, ys); }

    // xs must be strictly increasing and the same length as ys. Fewer than 2 points still works
    // (a single point is a constant; empty is invalid).
    void build(const std::vector<double>& xs, const std::vector<double>& ys) {
        m_x = xs;
        m_y = ys;
        m_m.clear();
        const std::size_t n = m_x.size();
        if (n != m_y.size()) {
            m_x.clear();
            m_y.clear();
            return;
        }
        if (n < 2) {
            m_m.assign(n, 0.0);
            return;
        }

        // Secant slopes between consecutive points.
        std::vector<double> delta(n - 1);
        for (std::size_t i = 0; i + 1 < n; ++i) {
            delta[i] = (m_y[i + 1] - m_y[i]) / (m_x[i + 1] - m_x[i]);
        }

        // Initial tangents: endpoints use the one-sided secant, interior uses the average.
        m_m.assign(n, 0.0);
        m_m[0] = delta[0];
        m_m[n - 1] = delta[n - 2];
        for (std::size_t i = 1; i + 1 < n; ++i) {
            m_m[i] = (delta[i - 1] + delta[i]) * 0.5;
        }

        // Fritsch-Carlson: clamp tangents so every segment is monotone (no overshoot).
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (delta[i] == 0.0) {
                m_m[i] = 0.0;
                m_m[i + 1] = 0.0;
                continue;
            }
            const double alpha = m_m[i] / delta[i];
            const double beta = m_m[i + 1] / delta[i];
            if (alpha < 0.0) {
                m_m[i] = 0.0;
            }
            if (beta < 0.0) {
                m_m[i + 1] = 0.0;
            }
            const double a2 = m_m[i] / delta[i];
            const double b2 = m_m[i + 1] / delta[i];
            const double s = a2 * a2 + b2 * b2;
            if (s > 9.0) {
                const double tau = 3.0 / std::sqrt(s);
                m_m[i] = tau * a2 * delta[i];
                m_m[i + 1] = tau * b2 * delta[i];
            }
        }
    }

    bool valid() const { return m_x.size() >= 1 && m_x.size() == m_y.size(); }
    std::size_t size() const { return m_x.size(); }
    double minX() const { return m_x.empty() ? 0.0 : m_x.front(); }
    double maxX() const { return m_x.empty() ? 0.0 : m_x.back(); }

    // Interpolated value at x. Clamps to the first/last sample outside [minX, maxX].
    double eval(double x) const {
        const std::size_t n = m_x.size();
        if (n == 0) {
            return 0.0;
        }
        if (n == 1 || x <= m_x.front()) {
            return m_y.front();
        }
        if (x >= m_x.back()) {
            return m_y.back();
        }
        // Find the segment [x[i], x[i+1]) containing x.
        const std::size_t hi = static_cast<std::size_t>(
            std::upper_bound(m_x.begin(), m_x.end(), x) - m_x.begin());
        const std::size_t i = hi - 1;
        const double h = m_x[i + 1] - m_x[i];
        const double t = (x - m_x[i]) / h;
        const double t2 = t * t;
        const double t3 = t2 * t;
        // Hermite basis functions.
        const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        const double h10 = t3 - 2.0 * t2 + t;
        const double h01 = -2.0 * t3 + 3.0 * t2;
        const double h11 = t3 - t2;
        return h00 * m_y[i] + h10 * h * m_m[i] + h01 * m_y[i + 1] + h11 * h * m_m[i + 1];
    }

private:
    std::vector<double> m_x;
    std::vector<double> m_y;
    std::vector<double> m_m; // per-point tangents
};

} // namespace maz::math
