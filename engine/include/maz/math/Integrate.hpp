#pragma once

#include <cmath>
#include <cstddef>

// maz::math numerical integration — estimate the definite integral of a function you can only evaluate
// pointwise. The everyday need behind measuring the ARC LENGTH of a curve (integrate the speed), the AREA
// under a response/response-time curve, the WORK done by a varying force, the expected value of a
// distribution, or any "sum up a continuous quantity" the engine can't do in closed form. Two methods:
// composite Gauss-Legendre (5-point per panel — spectacularly accurate for smooth functions, and EXACT for
// polynomials up to degree 9) and adaptive Simpson (spends samples where the function wiggles, to a caller
// tolerance). Both are templated on any callable double(double). Computes in double for accuracy. Godot has
// no general numeric integrator. Header-only, std-only, deterministic.
namespace maz::math {

namespace integ_detail {
// 5-point Gauss-Legendre nodes/weights on [-1, 1] (exact to degree 9).
constexpr double kGLx[5] = {0.0, -0.5384693101056831, 0.5384693101056831, -0.9061798459386640,
                            0.9061798459386640};
constexpr double kGLw[5] = {0.5688888888888889, 0.4786286704993665, 0.4786286704993665, 0.2369268850561891,
                            0.2369268850561891};

template <typename F>
inline double gl5(F&& f, double a, double b) {
    const double xm = 0.5 * (a + b);
    const double xr = 0.5 * (b - a);
    double s = 0.0;
    for (int i = 0; i < 5; ++i) {
        s += kGLw[i] * f(xm + xr * kGLx[i]);
    }
    return xr * s;
}

inline double simpson(double a, double b, double fa, double fb, double fm) {
    return (b - a) / 6.0 * (fa + 4.0 * fm + fb);
}

template <typename F>
inline double adaptive(F&& f, double a, double b, double fa, double fb, double fm, double whole, double tol,
                       int depth) {
    const double m = 0.5 * (a + b);
    const double lm = 0.5 * (a + m), rm = 0.5 * (m + b);
    const double flm = f(lm), frm = f(rm);
    const double left = simpson(a, m, fa, fm, flm);
    const double right = simpson(m, b, fm, fb, frm);
    if (depth <= 0 || std::fabs(left + right - whole) <= 15.0 * tol) {
        return left + right + (left + right - whole) / 15.0; // Richardson-extrapolated
    }
    return adaptive(f, a, m, fa, fm, flm, left, tol * 0.5, depth - 1) +
           adaptive(f, m, b, fm, fb, frm, right, tol * 0.5, depth - 1);
}
} // namespace integ_detail

// Composite 5-point Gauss-Legendre integral of f over [a, b] with `panels` equal sub-intervals (>= 1).
// Exact (to rounding) for polynomials up to degree 9 with a single panel; use more panels for wilder f.
template <typename F>
inline double integrateGauss(F&& f, double a, double b, int panels = 1) {
    if (panels < 1) {
        panels = 1;
    }
    const double h = (b - a) / static_cast<double>(panels);
    double sum = 0.0;
    for (int i = 0; i < panels; ++i) {
        const double x0 = a + h * static_cast<double>(i);
        sum += integ_detail::gl5(f, x0, x0 + h);
    }
    return sum;
}

// Adaptive Simpson integral of f over [a, b] to absolute tolerance `tol`, refining only where needed.
template <typename F>
inline double integrateAdaptiveSimpson(F&& f, double a, double b, double tol = 1e-8, int maxDepth = 40) {
    const double fa = f(a), fb = f(b), fm = f(0.5 * (a + b));
    const double whole = integ_detail::simpson(a, b, fa, fb, fm);
    return integ_detail::adaptive(f, a, b, fa, fb, fm, whole, tol, maxDepth);
}

} // namespace maz::math
