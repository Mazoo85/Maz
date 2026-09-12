#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math clothoid / Euler spiral — the transition curve whose CURVATURE varies LINEARLY with arc length,
// κ(s) = κ0 + rate·s. It is the shape real roads, railways and racetracks use to connect a straight to a
// circular corner: because curvature ramps smoothly instead of jumping, a body following it feels no sudden
// sideways lurch (continuous lateral acceleration). Use it for smooth road/track geometry, camera and
// motion rails that must not snap between straight and curved sections, and procedural spiral shapes.
// The engine's Bézier/B-spline curves control position but not curvature directly; the clothoid is the
// curvature-first primitive, and Godot has no equivalent. Evaluated by integrating the unit-speed tangent
// θ(s) = θ0 + κ0·s + ½·rate·s² (Simpson's rule); with rate = 0 it degenerates exactly to a straight line
// (κ0 = 0) or a circular arc (κ0 ≠ 0). Header-only, std-only, deterministic.
namespace maz::math {

// The point at arc length `s` along the clothoid that starts at `p0` with heading `theta0` (radians),
// initial curvature `kappa0` and curvature rate `rate`. Arc length is measured from p0 (unit speed).
inline vec2 clothoidPoint(const vec2& p0, float theta0, float kappa0, float rate, float s) {
    // Integrate (cos θ, sin θ) from 0 to s with Simpson's rule over an even number of sub-steps.
    const float as = std::fabs(s);
    int steps = static_cast<int>(as * 64.0f) + 8;
    steps += steps & 1; // make even
    const double h = static_cast<double>(s) / static_cast<double>(steps);
    auto heading = [&](double u) {
        return static_cast<double>(theta0) + static_cast<double>(kappa0) * u +
               0.5 * static_cast<double>(rate) * u * u;
    };
    double sx = 0.0, sy = 0.0;
    for (int i = 0; i <= steps; ++i) {
        const double u = h * static_cast<double>(i);
        const double w = (i == 0 || i == steps) ? 1.0 : ((i & 1) ? 4.0 : 2.0);
        const double th = heading(u);
        sx += w * std::cos(th);
        sy += w * std::sin(th);
    }
    const double k = h / 3.0;
    return vec2(p0.x + static_cast<float>(k * sx), p0.y + static_cast<float>(k * sy));
}

// Heading (tangent angle, radians) at arc length `s`.
inline float clothoidHeading(float theta0, float kappa0, float rate, float s) {
    return theta0 + kappa0 * s + 0.5f * rate * s * s;
}

// Curvature at arc length `s` (signed; positive turns left in a Y-up frame).
inline float clothoidCurvature(float kappa0, float rate, float s) { return kappa0 + rate * s; }

// A polyline of `segments + 1` points sampling the clothoid over arc length [0, length]. Uses cumulative
// integration so cost is linear in the number of samples. `segments` is clamped to at least 1.
inline std::vector<vec2> clothoidPolyline(const vec2& p0, float theta0, float kappa0, float rate,
                                          float length, int segments) {
    if (segments < 1) {
        segments = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(segments + 1));
    out.push_back(p0);
    const double segLen = static_cast<double>(length) / static_cast<double>(segments);
    const int sub = 8; // Simpson sub-steps per output segment
    const double h = segLen / static_cast<double>(sub);
    double px = p0.x, py = p0.y;
    auto heading = [&](double u) {
        return static_cast<double>(theta0) + static_cast<double>(kappa0) * u +
               0.5 * static_cast<double>(rate) * u * u;
    };
    for (int seg = 0; seg < segments; ++seg) {
        const double s0 = static_cast<double>(seg) * segLen;
        double sx = 0.0, sy = 0.0;
        for (int i = 0; i <= sub; ++i) {
            const double u = s0 + h * static_cast<double>(i);
            const double w = (i == 0 || i == sub) ? 1.0 : ((i & 1) ? 4.0 : 2.0);
            const double th = heading(u);
            sx += w * std::cos(th);
            sy += w * std::sin(th);
        }
        const double k = h / 3.0;
        px += k * sx;
        py += k * sy;
        out.push_back(vec2(static_cast<float>(px), static_cast<float>(py)));
    }
    return out;
}

} // namespace maz::math
