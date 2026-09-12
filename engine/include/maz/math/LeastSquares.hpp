#pragma once

// maz::math least-squares curve fitting — recover the line or polynomial that best matches a cloud
// of (x, y) samples in the ordinary least-squares sense (minimizing the summed squared vertical
// error). This is the tool for turning noisy measurements into a smooth trend: fit a straight line
// to a scatter of points, calibrate a sensor/analog-stick response curve, model a difficulty ramp
// from playtest data, or smooth a jittery signal by fitting a low-degree polynomial. `fitLine`
// gives slope/intercept in closed form; `fitPolynomial` fits any degree via the normal equations
// (A^T A c = A^T y) solved with Gaussian elimination + partial pivoting. Both report R^2, the
// coefficient of determination (1 = perfect fit, 0 = no better than the mean). Godot exposes no
// curve-fitting to gameplay code, so this is a beyond-Godot numerics utility. Header-only, std-only,
// deterministic. Accumulation is done in double for conditioning; results are returned as float.
#include <cstddef>
#include <vector>

namespace maz::math {

// Straight-line fit y = slope * x + intercept.
struct LineFit {
    float slope = 0.0f;
    float intercept = 0.0f;
    float r2 = 0.0f;   // coefficient of determination in [.., 1]; 1 = perfect fit
    bool ok = false;   // false if inputs are invalid (mismatched/too few points, or all x equal)
};

// Polynomial fit: coeffs[0] + coeffs[1]*x + coeffs[2]*x^2 + ... (ascending powers).
struct PolyFit {
    std::vector<float> coeffs; // size degree+1 on success, empty on failure
    float r2 = 0.0f;
    bool ok = false;
};

namespace detail {

// Solve the m-by-m dense system a*x = b in place (a is row-major, size m*m; b size m) via Gaussian
// elimination with partial pivoting. Returns false if the matrix is (numerically) singular.
inline bool solveDense(std::vector<double>& a, std::vector<double>& b, std::size_t m) {
    for (std::size_t col = 0; col < m; ++col) {
        // Partial pivot: pick the row with the largest magnitude in this column.
        std::size_t pivot = col;
        double best = a[col * m + col] < 0.0 ? -a[col * m + col] : a[col * m + col];
        for (std::size_t r = col + 1; r < m; ++r) {
            const double v = a[r * m + col] < 0.0 ? -a[r * m + col] : a[r * m + col];
            if (v > best) {
                best = v;
                pivot = r;
            }
        }
        if (best < 1e-12) return false; // singular / rank-deficient
        if (pivot != col) {
            for (std::size_t k = 0; k < m; ++k) {
                const double t = a[col * m + k];
                a[col * m + k] = a[pivot * m + k];
                a[pivot * m + k] = t;
            }
            const double tb = b[col];
            b[col] = b[pivot];
            b[pivot] = tb;
        }
        // Eliminate below the pivot.
        const double diag = a[col * m + col];
        for (std::size_t r = col + 1; r < m; ++r) {
            const double factor = a[r * m + col] / diag;
            if (factor == 0.0) continue;
            for (std::size_t k = col; k < m; ++k) a[r * m + k] -= factor * a[col * m + k];
            b[r] -= factor * b[col];
        }
    }
    // Back-substitution.
    for (std::size_t i = m; i-- > 0;) {
        double sum = b[i];
        for (std::size_t k = i + 1; k < m; ++k) sum -= a[i * m + k] * b[k];
        b[i] = sum / a[i * m + i];
    }
    return true;
}

} // namespace detail

// Evaluate a polynomial (ascending-power coefficients) at x using Horner's method.
inline float evalPolynomial(const std::vector<float>& coeffs, float x) {
    double acc = 0.0;
    for (std::size_t i = coeffs.size(); i-- > 0;) acc = acc * static_cast<double>(x) + static_cast<double>(coeffs[i]);
    return static_cast<float>(acc);
}

// Ordinary least-squares straight-line fit. Needs >= 2 points with at least two distinct x values.
inline LineFit fitLine(const std::vector<float>& xs, const std::vector<float>& ys) {
    LineFit out;
    const std::size_t n = xs.size();
    if (n < 2 || ys.size() != n) return out;

    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(xs[i]);
        const double y = static_cast<double>(ys[i]);
        sx += x;
        sy += y;
        sxx += x * x;
        sxy += x * y;
    }
    const double dn = static_cast<double>(n);
    const double denom = dn * sxx - sx * sx; // n * variance(x)
    if (denom < 1e-12 && denom > -1e-12) return out; // all x identical -> vertical, undefined slope

    const double slope = (dn * sxy - sx * sy) / denom;
    const double intercept = (sy - slope * sx) / dn;

    // R^2 = 1 - SS_res / SS_tot.
    const double ybar = sy / dn;
    double ssRes = 0.0, ssTot = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(xs[i]);
        const double y = static_cast<double>(ys[i]);
        const double f = slope * x + intercept;
        ssRes += (y - f) * (y - f);
        ssTot += (y - ybar) * (y - ybar);
    }
    out.slope = static_cast<float>(slope);
    out.intercept = static_cast<float>(intercept);
    out.r2 = (ssTot < 1e-12) ? (ssRes < 1e-12 ? 1.0f : 0.0f) : static_cast<float>(1.0 - ssRes / ssTot);
    out.ok = true;
    return out;
}

// Least-squares polynomial fit of the given degree via the normal equations. Needs degree >= 0 and
// at least degree+1 points; returns ok=false if the system is rank-deficient (e.g. too few distinct
// x values for the requested degree).
inline PolyFit fitPolynomial(const std::vector<float>& xs, const std::vector<float>& ys, int degree) {
    PolyFit out;
    const std::size_t n = xs.size();
    if (degree < 0 || ys.size() != n) return out;
    const std::size_t m = static_cast<std::size_t>(degree) + 1; // number of coefficients
    if (n < m) return out;

    // Power sums S_k = sum x^k for k = 0 .. 2*degree, and moment sums T_j = sum x^j * y for j=0..degree.
    std::vector<double> powerSums(2 * static_cast<std::size_t>(degree) + 1, 0.0);
    std::vector<double> momentSums(m, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(xs[i]);
        const double y = static_cast<double>(ys[i]);
        double xp = 1.0;
        for (std::size_t k = 0; k < powerSums.size(); ++k) {
            powerSums[k] += xp;
            if (k < m) momentSums[k] += xp * y;
            xp *= x;
        }
    }

    // Assemble the (m x m) normal-equation matrix A[j][k] = S_{j+k}, rhs = momentSums.
    std::vector<double> a(m * m, 0.0);
    std::vector<double> b = momentSums;
    for (std::size_t j = 0; j < m; ++j)
        for (std::size_t k = 0; k < m; ++k) a[j * m + k] = powerSums[j + k];

    if (!detail::solveDense(a, b, m)) return out; // singular -> cannot fit

    out.coeffs.resize(m);
    for (std::size_t j = 0; j < m; ++j) out.coeffs[j] = static_cast<float>(b[j]);

    // R^2 against the fitted polynomial.
    double sy = 0.0;
    for (std::size_t i = 0; i < n; ++i) sy += static_cast<double>(ys[i]);
    const double ybar = sy / static_cast<double>(n);
    double ssRes = 0.0, ssTot = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double y = static_cast<double>(ys[i]);
        const double f = static_cast<double>(evalPolynomial(out.coeffs, xs[i]));
        ssRes += (y - f) * (y - f);
        ssTot += (y - ybar) * (y - ybar);
    }
    out.r2 = (ssTot < 1e-12) ? (ssRes < 1e-12 ? 1.0f : 0.0f) : static_cast<float>(1.0 - ssRes / ssTot);
    out.ok = true;
    return out;
}

} // namespace maz::math
