#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math tractrix — the "drag curve": the path traced by an object on a taut leash of length `a` as the
// hand holding the other end is dragged along a straight line (the directrix). Its defining property is that
// the leash is always TANGENT to the curve and always the same length `a` to the drag line — which is exactly
// how a towed trailer, a dog on a lead, a swinging pendant or a trailing camera-target lags behind a mover.
// (Spun around its asymptote it also generates the pseudosphere, the constant-negative-curvature surface.)
// Standard parametrisation, hand dragged along +x, object starting at (0, a):
//     x(t) = a·(t − tanh t),   y(t) = a·sech t = a / cosh t,   t >= 0.
// Godot has no such curve. Header-only, std-only, deterministic.
namespace maz::math {

// A point on the tractrix of leash length `a` at parameter `t` (t >= 0 trails to +x; t < 0 mirrors to −x).
inline vec2 tractrixPoint(float a, float t) {
    return vec2(a * (t - std::tanh(t)), a / std::cosh(t));
}

// The point on the drag line (the x-axis) that the leash currently runs to — i.e. where the tangent at
// parameter `t` meets y = 0. The segment from tractrixPoint(a,t) to here has length exactly `a`.
inline vec2 tractrixDragPoint(float a, float t) {
    // The tangent hits y=0 at x = a·t (the hand's position), independent of the sag.
    return vec2(a * t, 0.0f);
}

// Arc length of the tractrix from parameter 0 to `t` (t >= 0): a·ln(cosh t).
inline float tractrixArcLength(float a, float t) {
    return a * std::log(std::cosh(t));
}

// A polyline of `samples + 1` points sampling the tractrix over [tStart, tEnd]. `samples` >= 1.
inline std::vector<vec2> tractrixPolyline(float a, float tStart, float tEnd, int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = tStart + (tEnd - tStart) * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(tractrixPoint(a, t));
    }
    return out;
}

} // namespace maz::math
