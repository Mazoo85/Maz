#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math dense linear system solver — solve A x = b for a general NxN matrix, plus determinant and matrix
// inverse, via LU decomposition with partial pivoting. This is the numerical workhorse under least-squares
// FITTING (fit a plane/polynomial/curve to data through the normal equations), inverse-kinematics and
// physics CONSTRAINT solves (small dense Jacobian/impulse systems), colour-space and calibration transforms,
// barycentric/anywhere-interpolation setups, and any "n equations, n unknowns" that shows up in tools and
// gameplay math. The engine has RK4, quadrature, root-finding and polynomial roots but no general Ax=b
// solver; this fills that. Partial pivoting keeps it numerically stable and detects singular systems.
// Row-major matrices, double precision. Header-only, std-only, deterministic.
namespace maz::math {

namespace detail {
// In-place LU decomposition with partial pivoting. `A` becomes L (unit-diagonal, below) + U (on/above);
// `piv` is the row permutation, `sign` the permutation parity. Returns false if the matrix is singular.
inline bool luDecompose(std::vector<double>& A, int n, std::vector<int>& piv, int& sign) {
    piv.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        piv[static_cast<std::size_t>(i)] = i;
    }
    sign = 1;
    for (int k = 0; k < n; ++k) {
        double maxv = std::fabs(A[static_cast<std::size_t>(k) * static_cast<std::size_t>(n) + static_cast<std::size_t>(k)]);
        int p = k;
        for (int i = k + 1; i < n; ++i) {
            const double v = std::fabs(A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(k)]);
            if (v > maxv) {
                maxv = v;
                p = i;
            }
        }
        if (maxv < 1e-14) {
            return false; // singular
        }
        if (p != k) {
            for (int j = 0; j < n; ++j) {
                std::swap(A[static_cast<std::size_t>(k) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)],
                          A[static_cast<std::size_t>(p) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)]);
            }
            std::swap(piv[static_cast<std::size_t>(k)], piv[static_cast<std::size_t>(p)]);
            sign = -sign;
        }
        const double akk = A[static_cast<std::size_t>(k) * static_cast<std::size_t>(n) + static_cast<std::size_t>(k)];
        for (int i = k + 1; i < n; ++i) {
            const double f = A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(k)] / akk;
            A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(k)] = f;
            for (int j = k + 1; j < n; ++j) {
                A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)] -=
                    f * A[static_cast<std::size_t>(k) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)];
            }
        }
    }
    return true;
}

// Solve using a precomputed LU (`lu`, `piv`) for right-hand side `b`.
inline std::vector<double> luSolve(const std::vector<double>& lu, const std::vector<int>& piv, int n,
                                   const std::vector<double>& b) {
    std::vector<double> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        x[static_cast<std::size_t>(i)] = b[static_cast<std::size_t>(piv[static_cast<std::size_t>(i)])];
    }
    // Forward substitution (unit lower).
    for (int i = 0; i < n; ++i) {
        double s = x[static_cast<std::size_t>(i)];
        for (int j = 0; j < i; ++j) {
            s -= lu[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)] * x[static_cast<std::size_t>(j)];
        }
        x[static_cast<std::size_t>(i)] = s;
    }
    // Back substitution (upper).
    for (int i = n - 1; i >= 0; --i) {
        double s = x[static_cast<std::size_t>(i)];
        for (int j = i + 1; j < n; ++j) {
            s -= lu[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(j)] * x[static_cast<std::size_t>(j)];
        }
        x[static_cast<std::size_t>(i)] = s / lu[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(i)];
    }
    return x;
}
} // namespace detail

// Solve A x = b (A row-major, size n*n; b size n). Writes the solution to `x`; returns false if singular.
inline bool solveLinearSystem(std::vector<double> A, const std::vector<double>& b, int n, std::vector<double>& x) {
    if (n <= 0 || A.size() != static_cast<std::size_t>(n) * static_cast<std::size_t>(n) ||
        b.size() != static_cast<std::size_t>(n)) {
        return false;
    }
    std::vector<int> piv;
    int sign = 1;
    if (!detail::luDecompose(A, n, piv, sign)) {
        return false;
    }
    x = detail::luSolve(A, piv, n, b);
    return true;
}

// Determinant of an n*n row-major matrix (0 if singular).
inline double determinant(std::vector<double> A, int n) {
    if (n <= 0 || A.size() != static_cast<std::size_t>(n) * static_cast<std::size_t>(n)) {
        return 0.0;
    }
    std::vector<int> piv;
    int sign = 1;
    if (!detail::luDecompose(A, n, piv, sign)) {
        return 0.0;
    }
    double det = static_cast<double>(sign);
    for (int i = 0; i < n; ++i) {
        det *= A[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(i)];
    }
    return det;
}

// Inverse of an n*n row-major matrix into `inv`; returns false if singular.
inline bool invertMatrix(const std::vector<double>& A, int n, std::vector<double>& inv) {
    if (n <= 0 || A.size() != static_cast<std::size_t>(n) * static_cast<std::size_t>(n)) {
        return false;
    }
    std::vector<double> lu = A;
    std::vector<int> piv;
    int sign = 1;
    if (!detail::luDecompose(lu, n, piv, sign)) {
        return false;
    }
    inv.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(n), 0.0);
    std::vector<double> e(static_cast<std::size_t>(n));
    for (int col = 0; col < n; ++col) {
        for (int i = 0; i < n; ++i) {
            e[static_cast<std::size_t>(i)] = (i == col) ? 1.0 : 0.0;
        }
        const std::vector<double> xcol = detail::luSolve(lu, piv, n, e);
        for (int i = 0; i < n; ++i) {
            inv[static_cast<std::size_t>(i) * static_cast<std::size_t>(n) + static_cast<std::size_t>(col)] = xcol[static_cast<std::size_t>(i)];
        }
    }
    return true;
}

} // namespace maz::math
