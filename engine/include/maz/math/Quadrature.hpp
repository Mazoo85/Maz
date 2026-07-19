#pragma once

// maz::math numerical quadrature — compute the definite integral (the signed area under the curve) of
// an arbitrary scalar function f(x) over [a, b]. This is the counterpart to the ODE integrator
// (Integrator.hpp advances a state through time); quadrature instead sums up a function's area. It is
// the tool for arc length of a parametric path (integrate the speed |r'(t)|), work done by a varying
// force over a distance, the area/volume swept by a profile, a cumulative distribution from a density,
// or any "total accumulated quantity" where the integrand is known as code rather than as samples.
// Four rules are provided, cheapest to most accurate: composite trapezoid (O(h^2)), composite Simpson
// (O(h^4), exact for cubics), adaptive Simpson (recursively refines only where the function is hard,
// to a requested tolerance), and Romberg (Richardson extrapolation on the trapezoid rule for fast
// convergence on smooth integrands). The integrand is any callable taking and returning double; all
// accumulation is in double. Godot exposes no general function integrator to gameplay code, so this is
// a beyond-Godot numerics utility. Header-only, std-only, deterministic.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::math {

// Composite trapezoidal rule over [a, b] with n subintervals (clamped to >= 1). O(n) evaluations.
template <typename Func>
double integrateTrapezoid(Func f, double a, double b, int n) {
    if (n < 1) n = 1;
    const double h = (b - a) / static_cast<double>(n);
    double sum = 0.5 * (f(a) + f(b));
    for (int i = 1; i < n; ++i) sum += f(a + static_cast<double>(i) * h);
    return sum * h;
}

// Composite Simpson's rule over [a, b] with n subintervals (clamped to >= 2 and rounded up to even).
// O(n) evaluations, O(h^4) error — integrates any cubic exactly.
template <typename Func>
double integrateSimpson(Func f, double a, double b, int n) {
    if (n < 2) n = 2;
    if (n % 2 != 0) ++n; // Simpson needs an even number of subintervals
    const double h = (b - a) / static_cast<double>(n);
    double sum = f(a) + f(b);
    for (int i = 1; i < n; ++i) {
        const double x = a + static_cast<double>(i) * h;
        sum += (i % 2 == 1 ? 4.0 : 2.0) * f(x);
    }
    return sum * h / 3.0;
}

namespace detail {

// One recursive step of adaptive Simpson: compare the whole-interval Simpson estimate against the
// sum of its two halves; recurse only where they disagree by more than the local tolerance.
template <typename Func>
double adaptiveSimpsonRec(Func f, double a, double b, double fa, double fb, double fm, double whole,
                          double tol, int depth) {
    const double m = 0.5 * (a + b);
    const double lm = 0.5 * (a + m);
    const double rm = 0.5 * (m + b);
    const double flm = f(lm);
    const double frm = f(rm);
    const double left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
    const double right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
    const double both = left + right;
    if (depth <= 0 || std::fabs(both - whole) <= 15.0 * tol) {
        return both + (both - whole) / 15.0; // Richardson correction
    }
    return adaptiveSimpsonRec(f, a, m, fa, fm, flm, left, tol * 0.5, depth - 1)
         + adaptiveSimpsonRec(f, m, b, fm, fb, frm, right, tol * 0.5, depth - 1);
}

} // namespace detail

// Adaptive Simpson integration to an absolute tolerance. Refines subintervals only where the
// integrand is difficult, so a smooth region costs little and a spike is resolved automatically.
template <typename Func>
double integrateAdaptiveSimpson(Func f, double a, double b, double tol = 1e-8, int maxDepth = 40) {
    const double fa = f(a);
    const double fb = f(b);
    const double m = 0.5 * (a + b);
    const double fm = f(m);
    const double whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
    return detail::adaptiveSimpsonRec(f, a, b, fa, fb, fm, whole, tol, maxDepth);
}

// Romberg integration: repeatedly halve the trapezoid step and Richardson-extrapolate, giving rapid
// convergence on smooth integrands. `levels` is the number of refinement rows (clamped to >= 1);
// each level roughly doubles the sample count. Returns the most-extrapolated estimate.
template <typename Func>
double integrateRomberg(Func f, double a, double b, int levels = 8) {
    if (levels < 1) levels = 1;
    std::vector<double> row(static_cast<std::size_t>(levels), 0.0);
    double h = b - a;
    row[0] = 0.5 * h * (f(a) + f(b)); // trapezoid with a single interval
    for (int i = 1; i < levels; ++i) {
        h *= 0.5;
        // Refine the trapezoid estimate by adding the new midpoints.
        double sum = 0.0;
        const std::int64_t pts = static_cast<std::int64_t>(1) << (i - 1);
        for (std::int64_t k = 1; k <= pts; ++k)
            sum += f(a + static_cast<double>(2 * k - 1) * h);
        double prev = row[0];              // R[i-1][0]
        row[0] = 0.5 * row[0] + h * sum;   // R[i][0]
        double pow4 = 4.0;
        for (int j = 1; j <= i; ++j) {
            const double cur = row[static_cast<std::size_t>(j)]; // old R[i-1][j], saved for next j
            row[static_cast<std::size_t>(j)] =
                (pow4 * row[static_cast<std::size_t>(j - 1)] - prev) / (pow4 - 1.0);
            prev = cur;
            pow4 *= 4.0;
        }
    }
    return row[static_cast<std::size_t>(levels - 1)];
}

} // namespace maz::math
