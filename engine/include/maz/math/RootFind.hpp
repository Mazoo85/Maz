#pragma once

// maz::math root finding — solve f(x) = 0 for an arbitrary scalar function supplied as a callable.
// This is the inverse-problem companion to the numerics family: Polynomial.hpp solves *known*
// quadratics/cubics in closed form, but many gameplay questions reduce to "find the x where this
// custom function crosses zero" — the launch angle that lands a projectile on a moving target, the
// time a value curve first hits a threshold, the parameter along a path nearest a point (root of the
// distance derivative), or inverting any monotonic response curve. Four methods are provided:
// bisection (bracketing, guaranteed to converge if the endpoints straddle a root, linear), Newton-
// Raphson (needs the derivative, quadratic convergence when it converges), secant (derivative-free,
// superlinear), and Brent (bracketing hybrid of bisection + secant + inverse-quadratic interpolation
// — robust like bisection but usually much faster; the recommended default when a bracket is known).
// The function (and derivative) are any callable taking and returning double. Godot exposes no root
// finder to gameplay code, so this is a beyond-Godot numerics utility. Header-only, std-only.
#include <cmath>
#include <utility>

namespace maz::math {

struct RootResult {
    double root = 0.0;
    bool converged = false;
    int iterations = 0;
};

// Bisection: requires f(a) and f(b) to have opposite signs (a bracket). Halves the interval each step;
// slow but utterly reliable. Converges when |f| or the half-width falls below tol.
template <typename Func>
RootResult findRootBisection(Func f, double a, double b, double tol = 1e-10, int maxIter = 200) {
    RootResult out;
    double fa = f(a);
    double fb = f(b);
    if ((fa > 0.0 && fb > 0.0) || (fa < 0.0 && fb < 0.0)) return out; // no sign change -> no bracket
    if (fa == 0.0) { out.root = a; out.converged = true; return out; }
    if (fb == 0.0) { out.root = b; out.converged = true; return out; }
    for (int i = 1; i <= maxIter; ++i) {
        const double m = 0.5 * (a + b);
        const double fm = f(m);
        out.root = m;
        out.iterations = i;
        if (std::fabs(fm) <= tol || 0.5 * (b - a) <= tol) {
            out.converged = true;
            return out;
        }
        if ((fa < 0.0 && fm < 0.0) || (fa > 0.0 && fm > 0.0)) {
            a = m;
            fa = fm;
        } else {
            b = m;
        }
    }
    return out;
}

// Newton-Raphson: x <- x - f(x)/f'(x). Fast (quadratic) near a simple root; needs the derivative and
// can diverge from a poor start or stall at a near-zero derivative (reported as not converged).
template <typename Func, typename Deriv>
RootResult findRootNewton(Func f, Deriv df, double x0, double tol = 1e-10, int maxIter = 100) {
    RootResult out;
    double x = x0;
    for (int i = 1; i <= maxIter; ++i) {
        const double fx = f(x);
        out.root = x;
        out.iterations = i;
        if (std::fabs(fx) <= tol) {
            out.converged = true;
            return out;
        }
        const double dfx = df(x);
        if (std::fabs(dfx) < 1e-14) return out; // flat -> Newton step undefined
        const double xNext = x - fx / dfx;
        out.root = xNext;
        if (std::fabs(xNext - x) <= tol) {
            out.converged = true;
            return out;
        }
        x = xNext;
    }
    return out;
}

// Secant: like Newton but approximates the derivative from the last two points, so no derivative is
// needed. Superlinear convergence. Needs two starting guesses.
template <typename Func>
RootResult findRootSecant(Func f, double x0, double x1, double tol = 1e-10, int maxIter = 100) {
    RootResult out;
    double f0 = f(x0);
    double f1 = f(x1);
    for (int i = 1; i <= maxIter; ++i) {
        const double denom = f1 - f0;
        out.root = x1;
        out.iterations = i;
        if (std::fabs(f1) <= tol) {
            out.converged = true;
            return out;
        }
        if (std::fabs(denom) < 1e-14) return out; // secant slope ~0 -> step undefined
        const double x2 = x1 - f1 * (x1 - x0) / denom;
        out.root = x2;
        if (std::fabs(x2 - x1) <= tol) {
            out.converged = true;
            return out;
        }
        x0 = x1;
        f0 = f1;
        x1 = x2;
        f1 = f(x1);
    }
    return out;
}

// Brent's method: a bracketing solver that blends inverse-quadratic interpolation, the secant step,
// and bisection, choosing bisection whenever the faster steps misbehave. Requires an initial bracket
// (opposite signs at a, b). Robust and typically the fastest of these — the recommended default.
template <typename Func>
RootResult findRootBrent(Func f, double a, double b, double tol = 1e-12, int maxIter = 200) {
    RootResult out;
    double fa = f(a);
    double fb = f(b);
    if ((fa > 0.0 && fb > 0.0) || (fa < 0.0 && fb < 0.0)) return out; // no bracket
    if (fa == 0.0) { out.root = a; out.converged = true; return out; }
    if (fb == 0.0) { out.root = b; out.converged = true; return out; }

    if (std::fabs(fa) < std::fabs(fb)) { // ensure |f(b)| <= |f(a)|
        std::swap(a, b);
        std::swap(fa, fb);
    }
    double c = a;
    double fc = fa;
    double d = c; // previous step's contrapoint; seeded, only used once mflag is false
    bool mflag = true;

    for (int i = 1; i <= maxIter; ++i) {
        out.iterations = i;
        if (fb == 0.0 || std::fabs(b - a) <= tol) {
            out.root = b;
            out.converged = true;
            return out;
        }
        double s;
        if (fa != fc && fb != fc) {
            // Inverse quadratic interpolation.
            s = a * fb * fc / ((fa - fb) * (fa - fc)) + b * fa * fc / ((fb - fa) * (fb - fc))
              + c * fa * fb / ((fc - fa) * (fc - fb));
        } else {
            // Secant step.
            s = b - fb * (b - a) / (fb - fa);
        }
        const double lo = 0.25 * (3.0 * a + b);
        const bool sOutside = (s < std::fmin(lo, b)) || (s > std::fmax(lo, b));
        if (sOutside
            || (mflag && std::fabs(s - b) >= 0.5 * std::fabs(b - c))
            || (!mflag && std::fabs(s - b) >= 0.5 * std::fabs(c - d))
            || (mflag && std::fabs(b - c) < tol)
            || (!mflag && std::fabs(c - d) < tol)) {
            s = 0.5 * (a + b); // fall back to bisection
            mflag = true;
        } else {
            mflag = false;
        }
        const double fs = f(s);
        d = c;
        c = b;
        fc = fb;
        if ((fa < 0.0 && fs < 0.0) || (fa > 0.0 && fs > 0.0)) {
            a = s;
            fa = fs;
        } else {
            b = s;
            fb = fs;
        }
        if (std::fabs(fa) < std::fabs(fb)) {
            std::swap(a, b);
            std::swap(fa, fb);
        }
    }
    out.root = b;
    out.converged = std::fabs(fb) <= tol * 1e3; // report success if the residual is small
    return out;
}

} // namespace maz::math
