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

struct QuarticRoots {
    int count = 0;                        // 0..4 distinct real roots
    float x[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // sorted ascending
};

// Real roots of a*x^4 + b*x^3 + c*x^2 + d*x + e = 0 (distinct, sorted ascending). Falls back to the cubic
// solver when a is ~0. Method: the real roots of the derivative (a cubic, solved exactly) split the line into
// monotone intervals; within each we locate the at-most-one root by sign-change bisection, and detect double
// roots where the function grazes zero at a critical point. Robust and free of Ferrari's cancellation issues.
inline QuarticRoots solveQuartic(float a, float b, float c, float d, float e) {
    QuarticRoots out;
    if (std::fabs(static_cast<double>(a)) < 1e-12) {
        const CubicRoots cr = solveCubic(b, c, d, e);
        out.count = cr.count;
        for (int i = 0; i < cr.count; ++i) {
            out.x[i] = cr.x[i];
        }
        return out;
    }
    const double B = static_cast<double>(b) / a;
    const double C = static_cast<double>(c) / a;
    const double D = static_cast<double>(d) / a;
    const double E = static_cast<double>(e) / a;
    auto f = [&](double x) { return (((x + B) * x + C) * x + D) * x + E; };

    // Cauchy bound: every real root lies in [-M, M].
    const double M = 1.0 + std::max(std::max(std::fabs(B), std::fabs(C)), std::max(std::fabs(D), std::fabs(E)));

    // Critical points (roots of the derivative 4x^3 + 3B x^2 + 2C x + D), sorted ascending.
    const CubicRoots cr = solveCubic(4.0f, static_cast<float>(3.0 * B), static_cast<float>(2.0 * C),
                                     static_cast<float>(D));
    double pts[5];
    int np = 0;
    pts[np++] = -M;
    for (int i = 0; i < cr.count; ++i) {
        const double cp = static_cast<double>(cr.x[i]);
        if (cp > -M && cp < M) {
            pts[np++] = cp;
        }
    }
    pts[np++] = M;

    double found[4];
    int nf = 0;
    auto addRoot = [&](double r) {
        for (int i = 0; i < nf; ++i) {
            if (std::fabs(found[i] - r) < 1e-4 * (1.0 + std::fabs(r))) {
                return;
            }
        }
        if (nf < 4) {
            found[static_cast<unsigned>(nf) & 3u] = r; // mask keeps the index provably in [0,3] for GCC
            ++nf;
        }
    };

    for (int i = 0; i + 1 < np; ++i) {
        double lo = pts[i], hi = pts[i + 1];
        double flo = f(lo), fhi = f(hi);
        if (flo == 0.0) {
            addRoot(lo);
        }
        if (flo * fhi < 0.0) {
            for (int it = 0; it < 200; ++it) {
                const double mid = 0.5 * (lo + hi);
                const double fm = f(mid);
                if (flo * fm <= 0.0) {
                    hi = mid;
                    fhi = fm;
                } else {
                    lo = mid;
                    flo = fm;
                }
            }
            addRoot(0.5 * (lo + hi));
        }
    }
    // Double roots: the function grazes zero at a critical point. The tolerance is relative to the magnitudes
    // of the individual polynomial terms at that point (a genuine root is a large cancellation among them),
    // which correctly rejects a positive local minimum that never reaches zero.
    for (int i = 0; i < cr.count; ++i) {
        const double cp = static_cast<double>(cr.x[i]);
        if (cp <= -M || cp >= M) {
            continue;
        }
        const double cp2 = cp * cp;
        const double termScale = cp2 * cp2 + std::fabs(B * cp2 * cp) + std::fabs(C * cp2) +
                                 std::fabs(D * cp) + std::fabs(E) + 1.0;
        if (std::fabs(f(cp)) < 1e-6 * termScale) {
            addRoot(cp);
        }
    }

    // Sort the (at most 4) roots with a tiny manual insertion sort. std::sort on such a small stack array
    // trips a libstdc++ -Warray-bounds false positive at -O3 (its threshold path appears to read found[16]);
    // the explicit clamp gives GCC a provable bound and the manual sort avoids that library path entirely.
    const int cnt = nf < 4 ? nf : 4;
    for (int i = 1; i < cnt; ++i) {
        const double key = found[i];
        int j = i - 1;
        while (j >= 0 && found[j] > key) {
            found[j + 1] = found[j];
            --j;
        }
        found[j + 1] = key;
    }
    for (int i = 0; i < cnt; ++i) {
        if (out.count == 0 ||
            std::fabs(static_cast<double>(out.x[out.count - 1]) - found[i]) > 1e-4 * (1.0 + std::fabs(found[i]))) {
            out.x[out.count++] = static_cast<float>(found[i]);
        }
    }
    return out;
}

} // namespace maz::math
