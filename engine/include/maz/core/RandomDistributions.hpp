#pragma once

#include <cmath>

// maz::core random distribution sampling — Poisson, exponential, and geometric variates on top of ANY
// uniform source. core::Random already gives you uniform floats, integer ranges, weighted picks, shuffles,
// and a Gaussian; these three fill the classic "event timing" gap that games lean on constantly:
//   * exponential(rate)  — the WAIT between independent random events (respawn gaps, next-drop timer,
//                          radioactive-decay-style spacing). Memoryless: the mean wait is 1/rate.
//   * poisson(lambda)    — HOW MANY independent events land in one fixed interval (enemies spawned this
//                          second, loot rolls, packets this tick). Mean and variance both == lambda.
//   * geometric(p)       — how many Bernoulli(p) TRIALS until the first success (crit-streak length, "keep
//                          rolling until a hit"). Mean == 1/p.
// Each is a free function template taking a callable `u` that returns a double in [0,1) — so it works with
// Random (`[&]{ return rng.nextDouble(); }`), Pcg32, or a deterministic test stub, with no hard dependency.
// Header-only, std-only, deterministic given the source. Godot exposes only uniform + normal RNG.
namespace maz::core {

// Continuous exponential variate with the given rate (>0). Mean = 1/rate. Always returns a value > 0.
// Inverse-CDF method: x = -ln(1-u) / rate.
template <class U>
double exponential(U&& u, double rate) {
    if (rate <= 0.0) {
        return 0.0;
    }
    double v = static_cast<double>(u()); // [0,1)
    // Guard the log: 1-v must stay in (0,1]. v==0 -> ln(1)=0 (fine); push v away from 1.
    if (v >= 1.0) {
        v = 0.9999999999999999;
    }
    return -std::log(1.0 - v) / rate;
}

// Discrete Poisson variate with the given mean lambda (>=0). Mean == variance == lambda. Returns >= 0.
// Knuth's product method for moderate lambda; a rounded-normal approximation once exp(-lambda) would
// underflow (lambda large), which is the standard high-lambda fallback.
template <class U>
int poisson(U&& u, double lambda) {
    if (lambda <= 0.0) {
        return 0;
    }
    if (lambda < 500.0) {
        // Knuth: multiply uniforms until the running product drops below exp(-lambda). O(lambda) expected.
        const double limit = std::exp(-lambda);
        double product = 1.0;
        int k = 0;
        do {
            ++k;
            product *= static_cast<double>(u());
        } while (product > limit);
        return k - 1;
    }
    // Large lambda: round a normal(lambda, sqrt(lambda)) sample (Box-Muller), clamped at 0.
    double u1 = static_cast<double>(u());
    const double u2 = static_cast<double>(u());
    if (u1 <= 0.0) {
        u1 = 2.220446049250313e-16; // avoid log(0)
    }
    const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    const double x = lambda + std::sqrt(lambda) * z;
    const long r = std::lround(x);
    return r < 0 ? 0 : static_cast<int>(r);
}

// Discrete geometric variate: number of Bernoulli(p) trials until (and including) the first success.
// p in (0,1]. Mean = 1/p. Returns >= 1. Inverse-CDF: ceil( ln(1-u) / ln(1-p) ).
template <class U>
int geometric(U&& u, double p) {
    if (p >= 1.0) {
        return 1; // certain success on the first trial
    }
    if (p <= 0.0) {
        return 0; // degenerate: success never happens
    }
    double v = static_cast<double>(u());
    if (v >= 1.0) {
        v = 0.9999999999999999;
    }
    const double n = std::log(1.0 - v) / std::log(1.0 - p);
    const double c = std::ceil(n);
    return c < 1.0 ? 1 : static_cast<int>(c);
}

} // namespace maz::core
