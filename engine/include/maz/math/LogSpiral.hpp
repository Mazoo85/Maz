#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math logarithmic (equiangular) spiral — the growth spiral of nautilus shells, sunflower seed heads,
// galaxy arms and hurricanes: r(θ) = a·e^(b·θ), so the radius multiplies by a constant factor for every fixed
// turn. Its defining trait is that the curve crosses every ray from the centre at the SAME angle (hence
// "equiangular"), which also makes it SELF-SIMILAR — zooming in reproduces the same spiral rotated. Use it
// for procedural shells/horns, spiral galaxies and vortices, spiral camera or motion paths, and radial UI
// layouts. Godot has no spiral primitive. `a` sets the starting radius (at θ = 0) and `b` the tightness
// (b = 0 degenerates to a circle; larger |b| unwinds faster; sign flips the winding direction). Returns a
// polyline ready for the line/polygon renderer. Header-only, std-only, deterministic.
namespace maz::math {

// A single point on the logarithmic spiral centred at `center` with scale `a` and growth rate `b`, at
// angle `theta` (radians). r = a·e^(b·θ).
inline vec2 logSpiralPoint(const vec2& center, float a, float b, float theta) {
    const float r = a * std::exp(b * theta);
    return vec2(center.x + r * std::cos(theta), center.y + r * std::sin(theta));
}

// Tangent (unnormalised derivative dP/dθ) of the spiral at `theta` — useful for orienting things along it.
inline vec2 logSpiralTangent(float a, float b, float theta) {
    const float r = a * std::exp(b * theta);
    // dP/dθ = b·r·(cosθ, sinθ) + r·(−sinθ, cosθ).
    const float ct = std::cos(theta), st = std::sin(theta);
    return vec2(r * (b * ct - st), r * (b * st + ct));
}

// A polyline of `samples + 1` points sampling the spiral over the angle range [thetaStart, thetaEnd].
// `samples` is clamped to at least 1.
inline std::vector<vec2> logSpiralPolyline(const vec2& center, float a, float b, float thetaStart,
                                           float thetaEnd, int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = thetaStart + (thetaEnd - thetaStart) * static_cast<float>(i) /
                                         static_cast<float>(samples);
        out.push_back(logSpiralPoint(center, a, b, t));
    }
    return out;
}

} // namespace maz::math
