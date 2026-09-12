#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math Nelder-Mead downhill simplex — minimise a scalar function of several variables WITHOUT needing
// its derivatives. You hand it a function f(x) (x is a vector of parameters) and a starting guess; it walks a
// simplex (a blob of n+1 points) downhill — reflecting, expanding, and contracting — until it settles in a
// minimum. Derivative-free means f can be anything you can evaluate: a simulation score, a fit error, a
// procedural-generation quality metric, a physics residual.
//
// This complements what the engine already has — RootFind (1-D root finding), SimulatedAnnealing (stochastic
// global search), Minimax (game trees) — with the workhorse for small, smooth, deterministic local
// minimisation: fitting a parametric curve/easing to sampled data, auto-tuning controller or spring gains,
// solving a few-parameter constraint (aim/IK-style) by minimising a residual, or calibrating a model. Neither
// Godot nor Unity ships a general optimiser. Header-only, std-only, deterministic (no RNG), double precision.
namespace maz::math {

struct NelderMeadResult {
    std::vector<double> x;   // best point found
    double fx = 0.0;         // f(x) there
    int iterations = 0;      // function-improving iterations performed (summed across restarts)
    bool converged = false;  // true if the simplex shrank below tolerance (vs hitting the iteration cap)
};

struct NelderMeadOptions {
    double step = 1.0;       // initial simplex edge length (how far the starting points spread from x0)
    double alpha = 1.0;      // reflection coefficient
    double gamma = 2.0;      // expansion coefficient
    double rho = 0.5;        // contraction coefficient
    double sigma = 0.5;      // shrink coefficient
    int maxIterations = 2000;// per-restart iteration cap
    int restarts = 2;        // times to rebuild the simplex around the best point and re-run (robustness)
    double xtol = 1e-10;     // converged when the simplex vertices span less than this
    double ftol = 1e-12;     // ...and the function values across the simplex span less than this
};

namespace nm_detail {
inline double dist2(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double d = a[i] - b[i];
        s += d * d;
    }
    return s;
}
} // namespace nm_detail

// Minimise `f` starting from `x0`. `f` is any callable taking a const std::vector<double>& and returning
// double. Returns the best point, its value, the iteration count, and whether it converged to tolerance.
template <typename F>
inline NelderMeadResult nelderMead(F&& f, const std::vector<double>& x0, const NelderMeadOptions& opt = {}) {
    const std::size_t n = x0.size();
    NelderMeadResult result;
    if (n == 0) {
        return result;
    }

    std::vector<std::vector<double>> simplex(n + 1);
    std::vector<double> fval(n + 1, 0.0);
    std::vector<double> best = x0;
    double bestF = f(x0);
    int totalIters = 0;
    bool converged = false;

    for (int restart = 0; restart <= opt.restarts; ++restart) {
        // Build the simplex around the current best: vertex 0 = best, vertex i = best nudged along axis i-1.
        simplex[0] = best;
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<double> v = best;
            const double h = (v[i] != 0.0) ? opt.step * 0.05 * v[i] : opt.step;
            v[i] += (h != 0.0) ? h : opt.step;
            simplex[i + 1] = v;
        }
        for (std::size_t i = 0; i <= n; ++i) {
            fval[i] = f(simplex[i]);
        }

        bool thisConverged = false;
        int iter = 0;
        for (; iter < opt.maxIterations; ++iter) {
            // Order vertices by ascending f: find best (lo), worst (hi), second-worst (nhi).
            std::size_t lo = 0, hi = 0, nhi = 0;
            for (std::size_t i = 1; i <= n; ++i) {
                if (fval[i] < fval[lo]) lo = i;
            }
            hi = lo;
            for (std::size_t i = 0; i <= n; ++i) {
                if (fval[i] > fval[hi]) hi = i;
            }
            nhi = (hi == 0) ? 1 : 0;
            for (std::size_t i = 0; i <= n; ++i) {
                if (i != hi && fval[i] > fval[nhi]) nhi = i;
            }

            // Convergence: both the point spread and the value spread are tiny.
            double maxSpan = 0.0;
            for (std::size_t i = 0; i <= n; ++i) {
                const double d2 = nm_detail::dist2(simplex[i], simplex[lo]);
                if (d2 > maxSpan) maxSpan = d2;
            }
            const double fspan = std::fabs(fval[hi] - fval[lo]);
            if (std::sqrt(maxSpan) < opt.xtol && fspan < opt.ftol) {
                thisConverged = true;
                break;
            }

            // Centroid of all vertices except the worst.
            std::vector<double> centroid(n, 0.0);
            for (std::size_t i = 0; i <= n; ++i) {
                if (i == hi) continue;
                for (std::size_t k = 0; k < n; ++k) centroid[k] += simplex[i][k];
            }
            for (std::size_t k = 0; k < n; ++k) centroid[k] /= static_cast<double>(n);

            // Reflection.
            std::vector<double> xr(n);
            for (std::size_t k = 0; k < n; ++k) xr[k] = centroid[k] + opt.alpha * (centroid[k] - simplex[hi][k]);
            const double fr = f(xr);

            if (fr < fval[lo]) {
                // Expansion.
                std::vector<double> xe(n);
                for (std::size_t k = 0; k < n; ++k) xe[k] = centroid[k] + opt.gamma * (xr[k] - centroid[k]);
                const double fe = f(xe);
                if (fe < fr) {
                    simplex[hi] = xe;
                    fval[hi] = fe;
                } else {
                    simplex[hi] = xr;
                    fval[hi] = fr;
                }
            } else if (fr < fval[nhi]) {
                simplex[hi] = xr; // accept reflection
                fval[hi] = fr;
            } else {
                // Contraction (outside if the reflection at least beat the worst, else inside).
                bool doShrink = false;
                if (fr < fval[hi]) {
                    std::vector<double> xc(n);
                    for (std::size_t k = 0; k < n; ++k) xc[k] = centroid[k] + opt.rho * (xr[k] - centroid[k]);
                    const double fc = f(xc);
                    if (fc <= fr) {
                        simplex[hi] = xc;
                        fval[hi] = fc;
                    } else {
                        doShrink = true;
                    }
                } else {
                    std::vector<double> xc(n);
                    for (std::size_t k = 0; k < n; ++k) xc[k] = centroid[k] + opt.rho * (simplex[hi][k] - centroid[k]);
                    const double fc = f(xc);
                    if (fc < fval[hi]) {
                        simplex[hi] = xc;
                        fval[hi] = fc;
                    } else {
                        doShrink = true;
                    }
                }
                if (doShrink) {
                    for (std::size_t i = 0; i <= n; ++i) {
                        if (i == lo) continue;
                        for (std::size_t k = 0; k < n; ++k)
                            simplex[i][k] = simplex[lo][k] + opt.sigma * (simplex[i][k] - simplex[lo][k]);
                        fval[i] = f(simplex[i]);
                    }
                }
            }
        }
        totalIters += iter;

        // Adopt the best vertex of this simplex as the seed for a possible restart.
        std::size_t lo = 0;
        for (std::size_t i = 1; i <= n; ++i) {
            if (fval[i] < fval[lo]) lo = i;
        }
        if (fval[lo] < bestF) {
            bestF = fval[lo];
            best = simplex[lo];
        }
        converged = thisConverged;
    }

    result.x = best;
    result.fx = bestF;
    result.iterations = totalIters;
    result.converged = converged;
    return result;
}

} // namespace maz::math
