#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math catenary — the shape a uniform flexible chain, rope, cable or wire takes when hung between two
// points under gravity: y = a·cosh(x/a). It is NOT a parabola (a common mistake); the difference is visible
// on rope bridges, power lines, hanging chains, tent ridges, mooring cables and swinging vines. Given two
// anchor points and a rope LENGTH longer than the straight-line gap, this solves for the unique catenary that
// passes through both anchors with exactly that much rope, then samples it — everything a game needs to draw
// a sagging rope/cable procedurally. Godot has no catenary helper. The solve is a 1-D root-find on the
// catenary parameter `a`; the sag grows as the rope lengthens. Header-only, std-only, deterministic.
namespace maz::math {

// A solved catenary y(x) = a·cosh((x - x0)/a) + c. `a` is the shape parameter (large = taut/flat, small =
// deep sag); (x0, a+c) is the lowest point of the curve.
struct Catenary {
    float a = 1.0f;
    float x0 = 0.0f;
    float c = 0.0f;
};

// Height of the solved catenary at world x.
inline float catenaryHeight(const Catenary& cat, float x) {
    return cat.a * std::cosh((x - cat.x0) / cat.a) + cat.c;
}

// Arc length of the catenary between x = xa and x = xb (xa <= xb): a·(sinh((xb-x0)/a) - sinh((xa-x0)/a)).
inline float catenaryArcLength(const Catenary& cat, float xa, float xb) {
    return cat.a * (std::sinh((xb - cat.x0) / cat.a) - std::sinh((xa - cat.x0) / cat.a));
}

// Solve for the catenary through `p1` and `p2` carrying a rope of total length `ropeLength`. Returns false
// (leaving `out` unchanged) if the anchors are (near-)vertically aligned or the rope is not longer than the
// straight-line distance between the anchors (a taut/too-short rope is a straight segment, not a catenary).
inline bool solveCatenary(const vec2& p1, const vec2& p2, float ropeLength, Catenary& out) {
    const double h = static_cast<double>(p2.x) - static_cast<double>(p1.x); // signed horizontal span
    const double v = static_cast<double>(p2.y) - static_cast<double>(p1.y); // signed vertical span
    const double habs = std::fabs(h);
    if (habs < 1e-6) {
        return false; // (near-)vertical: catenary is degenerate
    }
    const double L = static_cast<double>(ropeLength);
    const double straight = std::sqrt(habs * habs + v * v);
    if (L <= straight * (1.0 + 1e-6)) {
        return false; // rope not longer than the gap -> straight, no sag
    }
    // Solve  2a·sinh(h/(2a)) = sqrt(L^2 - v^2)  for a > 0 (monotone decreasing from +inf to h in a).
    const double D = std::sqrt(L * L - v * v);
    auto f = [&](double a) { return 2.0 * a * std::sinh(habs / (2.0 * a)) - D; };
    double lo = 1e-6, hi = 1.0;
    while (f(hi) > 0.0) {
        hi *= 2.0; // grow until f(hi) < 0 (f decreases in a)
        if (hi > 1e12) {
            return false;
        }
    }
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) > 0.0) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const double a = 0.5 * (lo + hi);
    // m = (A+B)/2 with tanh(m) = v/L; then x0 = midpoint_x - a*m, c fixes the endpoint height.
    const double m = std::atanh(v / L);
    const double x0 = 0.5 * (static_cast<double>(p1.x) + static_cast<double>(p2.x)) - a * m;
    const double c = static_cast<double>(p1.y) - a * std::cosh((static_cast<double>(p1.x) - x0) / a);
    out.a = static_cast<float>(a);
    out.x0 = static_cast<float>(x0);
    out.c = static_cast<float>(c);
    return true;
}

// Sample the hanging rope between `p1` and `p2` as `samples + 1` points (in x order from p1.x to p2.x). If
// the rope cannot sag (too short / vertical anchors) the result is the straight segment p1..p2. `samples` is
// clamped to at least 1.
inline std::vector<vec2> catenaryPolyline(const vec2& p1, const vec2& p2, float ropeLength, int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    Catenary cat;
    const bool ok = solveCatenary(p1, p2, ropeLength, cat);
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = p1.x + (p2.x - p1.x) * t;
        if (ok) {
            out.push_back(vec2(x, catenaryHeight(cat, x)));
        } else {
            out.push_back(vec2(x, p1.y + (p2.y - p1.y) * t)); // straight fallback
        }
    }
    return out;
}

} // namespace maz::math
