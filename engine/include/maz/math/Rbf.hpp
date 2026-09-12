#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math::RbfInterpolator2D — radial basis function interpolation of scattered 2D data. Given a handful
// of sample points, each with a value (a height, a weight, a colour channel, a displacement), it builds a
// single smooth field that passes EXACTLY through every sample and interpolates sensibly everywhere in
// between — no grid required. This is the standard tool for smooth image warping / morphing (pin control
// points and deform), terrain or influence maps from sparse measurements, scattered colour/weight blending,
// and smooth "attract toward these anchors" fields. It works by placing a radially-symmetric bump (Gaussian
// or multiquadric) on each sample and solving a small linear system for the bump weights so the sum hits
// every target value. Godot has no scattered-data interpolator. Header-only, std-only, deterministic;
// solves the weight system with Gaussian elimination (partial pivoting) at construction.
namespace maz::math {

enum class RbfKernel { Gaussian, Multiquadric, InverseMultiquadric };

namespace detail {
// Solve A x = b in place (A is n*n row-major, b length n). Returns false if (near) singular.
inline bool rbfSolve(std::vector<double>& A, std::vector<double>& b, int n) {
    for (int col = 0; col < n; ++col) {
        int piv = col;
        double best = std::fabs(A[static_cast<std::size_t>(col) * static_cast<std::size_t>(n) +
                                 static_cast<std::size_t>(col)]);
        for (int r = col + 1; r < n; ++r) {
            const double v = std::fabs(A[static_cast<std::size_t>(r) * static_cast<std::size_t>(n) +
                                         static_cast<std::size_t>(col)]);
            if (v > best) {
                best = v;
                piv = r;
            }
        }
        if (best < 1e-14) {
            return false;
        }
        if (piv != col) {
            for (int c = 0; c < n; ++c) {
                std::swap(A[static_cast<std::size_t>(col) * static_cast<std::size_t>(n) + static_cast<std::size_t>(c)],
                          A[static_cast<std::size_t>(piv) * static_cast<std::size_t>(n) + static_cast<std::size_t>(c)]);
            }
            std::swap(b[static_cast<std::size_t>(col)], b[static_cast<std::size_t>(piv)]);
        }
        const double diag = A[static_cast<std::size_t>(col) * static_cast<std::size_t>(n) + static_cast<std::size_t>(col)];
        for (int r = 0; r < n; ++r) {
            if (r == col) {
                continue;
            }
            const double f = A[static_cast<std::size_t>(r) * static_cast<std::size_t>(n) + static_cast<std::size_t>(col)] / diag;
            if (f == 0.0) {
                continue;
            }
            for (int c = col; c < n; ++c) {
                A[static_cast<std::size_t>(r) * static_cast<std::size_t>(n) + static_cast<std::size_t>(c)] -=
                    f * A[static_cast<std::size_t>(col) * static_cast<std::size_t>(n) + static_cast<std::size_t>(c)];
            }
            b[static_cast<std::size_t>(r)] -= f * b[static_cast<std::size_t>(col)];
        }
    }
    for (int i = 0; i < n; ++i) {
        b[static_cast<std::size_t>(i)] /= A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(i)];
    }
    return true;
}
} // namespace detail

class RbfInterpolator2D {
public:
    RbfInterpolator2D() = default;

    // Build an interpolant through `points` with the given `values`. `shape` scales the bump width.
    RbfInterpolator2D(const std::vector<vec2>& points, const std::vector<float>& values,
                      RbfKernel kernel = RbfKernel::Gaussian, float shape = 0.4f)
        : m_points(points), m_kernel(kernel), m_shape(shape) {
        build(values);
    }

    bool valid() const { return m_valid; }
    std::size_t size() const { return m_points.size(); }

    // Evaluate the field at `p`. Returns 0 if the interpolant failed to build or has no points.
    float eval(const vec2& p) const {
        if (!m_valid) {
            return 0.0f;
        }
        double acc = 0.0;
        for (std::size_t j = 0; j < m_points.size(); ++j) {
            const float dx = p.x - m_points[j].x, dy = p.y - m_points[j].y;
            acc += static_cast<double>(m_weights[j]) * static_cast<double>(phi(std::sqrt(dx * dx + dy * dy)));
        }
        return static_cast<float>(acc);
    }

private:
    float phi(float r) const {
        const float e = m_shape * r;
        switch (m_kernel) {
            case RbfKernel::Gaussian:
                return std::exp(-e * e);
            case RbfKernel::Multiquadric:
                return std::sqrt(e * e + 1.0f);
            case RbfKernel::InverseMultiquadric:
                return 1.0f / std::sqrt(e * e + 1.0f);
        }
        return 0.0f;
    }

    void build(const std::vector<float>& values) {
        const int n = static_cast<int>(m_points.size());
        if (n == 0 || static_cast<int>(values.size()) != n) {
            m_valid = false;
            return;
        }
        std::vector<double> A(static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
        std::vector<double> b(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                const float dx = m_points[static_cast<std::size_t>(i)].x - m_points[static_cast<std::size_t>(j)].x;
                const float dy = m_points[static_cast<std::size_t>(i)].y - m_points[static_cast<std::size_t>(j)].y;
                A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)] =
                    static_cast<double>(phi(std::sqrt(dx * dx + dy * dy)));
            }
            b[static_cast<std::size_t>(i)] = static_cast<double>(values[static_cast<std::size_t>(i)]);
        }
        if (!detail::rbfSolve(A, b, n)) {
            m_valid = false;
            return;
        }
        m_weights.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            m_weights[static_cast<std::size_t>(i)] = static_cast<float>(b[static_cast<std::size_t>(i)]);
        }
        m_valid = true;
    }

    std::vector<vec2> m_points;
    std::vector<float> m_weights;
    RbfKernel m_kernel = RbfKernel::Gaussian;
    float m_shape = 0.4f;
    bool m_valid = false;
};

} // namespace maz::math
