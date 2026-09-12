#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::math natural cubic spline — the classic globally-smooth interpolating spline: given a set of
// knots (x_i, y_i) with strictly increasing x, it builds the unique piecewise-cubic curve that passes
// exactly through every knot and is C2-continuous (continuous value, slope, AND curvature) everywhere,
// with the "natural" boundary condition (zero curvature at both ends). This differs from the engine's
// other curves: Curve2D is a Bezier (control points, not interpolation), anim::Curve is a keyframe
// track, and Catmull-Rom (in VectorOps) is only C1 and local. A natural cubic spline is the right tool
// for a smooth camera dolly through waypoints, a terrain cross-section through samples, or any "draw the
// smoothest curve through these points" need. Second derivatives are solved with the Thomas tridiagonal
// algorithm (O(n)). Header-only, std-only. Build a 2D/3D path by splining each component against a
// shared parameter.
namespace maz::math {

class CubicSpline {
public:
    CubicSpline() = default;
    CubicSpline(std::vector<float> xs, std::vector<float> ys) { build(std::move(xs), std::move(ys)); }

    // (Re)build from knots. xs must be strictly increasing and the same length as ys (>= 2). Anything
    // else leaves the spline empty (valid() == false).
    void build(std::vector<float> xs, std::vector<float> ys) {
        m_x.clear();
        m_y.clear();
        m_k.clear();
        const std::size_t n = xs.size();
        if (n < 2 || ys.size() != n) {
            return;
        }
        for (std::size_t i = 1; i < n; ++i) {
            if (!(xs[i] > xs[i - 1])) {
                return; // not strictly increasing
            }
        }
        m_x = std::move(xs);
        m_y = std::move(ys);
        m_k.assign(n, 0.0f); // second derivatives; natural BC keeps k[0] = k[n-1] = 0

        if (n >= 3) {
            std::vector<float> h(n - 1);
            for (std::size_t i = 0; i + 1 < n; ++i) {
                h[i] = m_x[i + 1] - m_x[i];
            }
            // Tridiagonal system for the interior second derivatives k[1..n-2].
            std::vector<float> diag(n, 0.0f);
            std::vector<float> sup(n, 0.0f);
            std::vector<float> rhs(n, 0.0f);
            for (std::size_t i = 1; i + 1 < n; ++i) {
                diag[i] = 2.0f * (h[i - 1] + h[i]);
                sup[i] = h[i];
                rhs[i] = 6.0f
                         * ((m_y[i + 1] - m_y[i]) / h[i] - (m_y[i] - m_y[i - 1]) / h[i - 1]);
            }
            // Thomas forward sweep (sub-diagonal at row i is h[i-1]); k[0]=k[n-1]=0 drop out at the ends.
            for (std::size_t i = 2; i + 1 < n; ++i) {
                const float w = h[i - 1] / diag[i - 1];
                diag[i] -= w * sup[i - 1];
                rhs[i] -= w * rhs[i - 1];
            }
            // Back substitution.
            m_k[n - 2] = rhs[n - 2] / diag[n - 2];
            for (std::size_t i = n - 2; i-- > 1;) {
                m_k[i] = (rhs[i] - sup[i] * m_k[i + 1]) / diag[i];
            }
        }
    }

    bool valid() const { return m_x.size() >= 2; }
    std::size_t knotCount() const { return m_x.size(); }

    // Interpolated value at x (clamped to the knot range). Returns 0 if the spline is empty.
    float eval(float x) const {
        if (!valid()) {
            return 0.0f;
        }
        x = std::clamp(x, m_x.front(), m_x.back());
        const std::size_t i = segment(x);
        const float h = m_x[i + 1] - m_x[i];
        const float A = (m_x[i + 1] - x) / h;
        const float B = (x - m_x[i]) / h;
        return A * m_y[i] + B * m_y[i + 1]
               + ((A * A * A - A) * m_k[i] + (B * B * B - B) * m_k[i + 1]) * (h * h) / 6.0f;
    }

    // First derivative dy/dx at x (clamped to the knot range).
    float derivative(float x) const {
        if (!valid()) {
            return 0.0f;
        }
        x = std::clamp(x, m_x.front(), m_x.back());
        const std::size_t i = segment(x);
        const float h = m_x[i + 1] - m_x[i];
        const float A = (m_x[i + 1] - x) / h;
        const float B = (x - m_x[i]) / h;
        return (m_y[i + 1] - m_y[i]) / h - (3.0f * A * A - 1.0f) / 6.0f * h * m_k[i]
               + (3.0f * B * B - 1.0f) / 6.0f * h * m_k[i + 1];
    }

    // Second derivative (curvature term) at x (clamped to the knot range).
    float secondDerivative(float x) const {
        if (!valid()) {
            return 0.0f;
        }
        x = std::clamp(x, m_x.front(), m_x.back());
        const std::size_t i = segment(x);
        const float h = m_x[i + 1] - m_x[i];
        const float A = (m_x[i + 1] - x) / h;
        const float B = (x - m_x[i]) / h;
        return A * m_k[i] + B * m_k[i + 1];
    }

private:
    std::vector<float> m_x;
    std::vector<float> m_y;
    std::vector<float> m_k;

    // Index of the segment [x[i], x[i+1]] containing (clamped) x.
    std::size_t segment(float x) const {
        const std::size_t last = m_x.size() - 1;
        if (x <= m_x[0]) {
            return 0;
        }
        if (x >= m_x[last]) {
            return last - 1;
        }
        const auto it = std::upper_bound(m_x.begin(), m_x.end(), x);
        std::size_t i = static_cast<std::size_t>(it - m_x.begin()) - 1;
        if (i > last - 1) {
            i = last - 1;
        }
        return i;
    }
};

} // namespace maz::math
