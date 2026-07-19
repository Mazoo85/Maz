#pragma once

#include <algorithm>
#include <cmath>

// maz::math analytic polynomial root solvers — closed-form real roots of quadratics and cubics. These
// are the exact building blocks behind ray/sphere and ray/quadric intersection, solving for the time a
// projectile reaches a height, inverting a cubic ease, and other "at what value does this curve equal
// zero" queries where an iterative solver would be slower and less precise. Godot exposes no general
// polynomial solver, so this is a beyond-Godot math utility. Quadratics use the numerically-stable
// form; cubics use Cardano with the trigonometric method for the three-real-root case. Roots come back
// sorted ascending and de-duplicated. Header-only, std-only.
namespace maz::math {

struct QuadraticRoots {
    int count = 0;   // 0, 1, or 2 real roots
    float x0 = 0.0f; // smaller root (or the single root)
    float x1 = 0.0f; // larger root
};

// Real roots of a*x^2 + b*x + c = 0. Falls back to the linear case when a is ~0.
inline QuadraticRoots solveQuadratic(float a, float b, float c) {
    QuadraticRoots out;
    const double A = a, B = b, C = c;
    const double eps = 1e-9;
    if (std::fabs(A) < eps) {
        if (std::fabs(B) < eps) {
            return out; // constant: no (or infinite) roots
        }
        out.count = 1;
        out.x0 = static_cast<float>(-C / B);
        out.x1 = out.x0;
        return out;
    }
    const double disc = B * B - 4.0 * A * C;
    if (disc < -eps) {
        return out; // no real roots
    }
    if (disc <= eps) {
        out.count = 1;
        out.x0 = static_cast<float>(-B / (2.0 * A));
        out.x1 = out.x0;
        return out;
    }
    const double s = std::sqrt(disc);
    // Stable: avoid cancellation by using q = -(b + sign(b)*sqrt(disc))/2.
    const double q = -0.5 * (B + (B >= 0.0 ? s : -s));
    double r0 = q / A;
    double r1 = C / q;
    if (r1 < r0) {
        std::swap(r0, r1);
    }
    out.count = 2;
    out.x0 = static_cast<float>(r0);
    out.x1 = static_cast<float>(r1);
    return out;
}

struct CubicRoots {
    int count = 0;    // 1, 2, or 3 distinct real roots
    float x[3] = {0.0f, 0.0f, 0.0f}; // sorted ascending
};

// Real roots of a*x^3 + b*x^2 + c*x + d = 0 (distinct, sorted ascending). Falls back to the quadratic
// solver when a is ~0.
inline CubicRoots solveCubic(float a, float b, float c, float d) {
    CubicRoots out;
    if (std::fabs(static_cast<double>(a)) < 1e-9) {
        const QuadraticRoots q = solveQuadratic(b, c, d);
        out.count = q.count;
        if (q.count >= 1) out.x[0] = q.x0;
        if (q.count == 2) out.x[1] = q.x1;
        return out;
    }

    const double B = static_cast<double>(b) / a;
    const double C = static_cast<double>(c) / a;
    const double D = static_cast<double>(d) / a;
    const double p = C - B * B / 3.0;               // depressed cubic t^3 + p t + q
    const double q = 2.0 * B * B * B / 27.0 - B * C / 3.0 + D;
    const double off = B / 3.0;                      // x = t - off
    const double disc = (q * q) / 4.0 + (p * p * p) / 27.0;
    const double eps = 1e-9;

    double roots[3];
    int n = 0;
    if (disc > eps) {
        const double s = std::sqrt(disc);
        const double u = std::cbrt(-q / 2.0 + s);
        const double v = std::cbrt(-q / 2.0 - s);
        roots[n++] = u + v - off; // one real root
    } else if (disc < -eps) {
        // Three distinct real roots (p < 0 here) via the trigonometric method.
        const double m = 2.0 * std::sqrt(-p / 3.0);
        double arg = (3.0 * q) / (2.0 * p) * std::sqrt(-3.0 / p);
        arg = std::clamp(arg, -1.0, 1.0);
        const double t = std::acos(arg) / 3.0;
        const double twoPi = 6.28318530717958648;
        for (int k = 0; k < 3; ++k) {
            roots[n++] = m * std::cos(t - twoPi * static_cast<double>(k) / 3.0) - off;
        }
    } else {
        // disc ~ 0: a multiple root.
        if (std::fabs(q) < eps && std::fabs(p) < eps) {
            roots[n++] = -off; // triple root
        } else {
            const double u = std::cbrt(-q / 2.0);
            roots[n++] = 2.0 * u - off; // single root
            roots[n++] = -u - off;      // double root (listed once)
        }
    }

    std::sort(roots, roots + n);
    // De-duplicate near-equal roots.
    for (int i = 0; i < n; ++i) {
        if (out.count == 0 || std::fabs(roots[i] - static_cast<double>(out.x[out.count - 1])) > 1e-5) {
            out.x[out.count++] = static_cast<float>(roots[i]);
        }
    }
    return out;
}

} // namespace maz::math
