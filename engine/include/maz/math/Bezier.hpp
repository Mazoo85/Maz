#pragma once

// A cubic Bezier curve — four control points where the curve passes through the
// endpoints p0 and p3 and is pulled toward the handles p1, p2 (which it does NOT
// interpolate). evaluate(t) is the Bernstein polynomial (t in [0,1], hitting p0 at
// 0 and p3 at 1 exactly; outside [0,1] it extrapolates); tangent(t) is the analytic
// derivative — the unnormalized travel direction, 3*(p1-p0) at t=0 and 3*(p3-p2) at
// t=1; sample(steps) yields a render polyline (steps+1 points, first == p0, last ==
// p3). The Godot Curve / path-handle analog, complementing iter35's interpolating
// Catmull-Rom (handles vs. through-points). Composes maz::math (vec3). De Casteljau
// subdivision, arc-length reparameterization, and a rational/quadratic variant are
// future refinements.

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <vector>
#include <cstddef>

namespace maz::math {

// Cubic Bezier position at t via the Bernstein basis:
// (1-t)^3 p0 + 3(1-t)^2 t p1 + 3(1-t) t^2 p2 + t^3 p3. At t=0 -> p0, at t=1 -> p3.
inline vec3 cubicBezier(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    const float u = 1.0f - t;
    const float uu = u * u;
    const float tt = t * t;
    return (uu * u) * p0 + (3.0f * uu * t) * p1 + (3.0f * u * tt) * p2 + (tt * t) * p3;
}

// dB/dt: the (unnormalized) direction of travel along the curve. At t=0 -> 3(p1-p0);
// at t=1 -> 3(p3-p2) — the tangent is 3x the handle vector at each end.
inline vec3 cubicBezierTangent(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) {
    const float u = 1.0f - t;
    return (3.0f * u * u) * (p1 - p0) + (6.0f * u * t) * (p2 - p1) + (3.0f * t * t) * (p3 - p2);
}

class CubicBezier {
public:
    // The four control points: p0/p3 are the endpoints the curve passes through,
    // p1/p2 are the handles it is pulled toward but does NOT interpolate.
    vec3 p0, p1, p2, p3;

    // Aggregate: default-constructible (CubicBezier{}) and brace-initializable from
    // its four control points (CubicBezier{p0,p1,p2,p3}); no user-declared ctor.

    // t is intended to be in [0,1] (p0 at 0, p3 at 1 exactly); it is NOT clamped, so
    // values outside [0,1] extrapolate.
    vec3 evaluate(float t) const { return cubicBezier(p0, p1, p2, p3, t); }

    // The (unnormalized) direction of travel along the curve at t.
    vec3 tangent(float t) const { return cubicBezierTangent(p0, p1, p2, p3, t); }

    // A polyline of steps+1 points at t = i/steps for i in [0, steps]. front() == p0
    // and back() == p3 exactly (t=0 and t=1 collapse to the endpoints).
    std::vector<vec3> sample(std::size_t steps) const {
        MAZ_ASSERT(steps >= 1, "CubicBezier::sample: steps >= 1");
        std::vector<vec3> out;
        out.reserve(steps + 1);
        for (std::size_t i = 0; i <= steps; ++i) {
            out.push_back(evaluate(static_cast<float>(i) / static_cast<float>(steps)));
        }
        return out;
    }
};

} // namespace maz::math
