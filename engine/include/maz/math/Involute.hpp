#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math involute of a circle — the curve traced by the end of a taut string as it unwinds from a circle.
// It is THE tooth-flank profile of real spur gears: two involute gears transmit rotation at a perfectly
// constant ratio because the contact normal always lies on the fixed "line of action" tangent to the base
// circles. Use it for mechanically-correct gears (as opposed to the decorative trapezoidal cog in
// render::shapes2d::gear), clock escapements, cam profiles, and unwinding-cable animation. Godot has no
// involute primitive. Parameterised by the unwinding angle `t` (radians): at t the string has unwound to the
// tangent point at angle t on the base circle and the free end sits a distance baseRadius·t away, perpendicular
// to that radius — so the swept arc length grows as baseRadius·t²/2 and the local radius of curvature is
// exactly baseRadius·t. Header-only, std-only, deterministic.
namespace maz::math {

// A single point on the involute of the circle centred at `center` with radius `baseRadius`, at unwinding
// angle `t` (radians). At t = 0 the point sits on the base circle at angle 0; growing t unwinds the string.
inline vec2 involutePoint(const vec2& center, float baseRadius, float t) {
    const float ct = std::cos(t), st = std::sin(t);
    return vec2(center.x + baseRadius * (ct + t * st), center.y + baseRadius * (st - t * ct));
}

// The tangent (unnormalised derivative dP/dt) of the involute at `t`. Equals baseRadius·t·(cos t, sin t) —
// i.e. it points radially outward from the base centre, so the curve's NORMAL is tangent to the base circle
// (the gear "line of action"). Zero at t = 0 (the cusp on the base circle).
inline vec2 involuteTangent(float baseRadius, float t) {
    return vec2(baseRadius * t * std::cos(t), baseRadius * t * std::sin(t));
}

// The point on the base circle from which the string is currently unwinding (the tangent / contact point)
// at angle `t`. The free end involutePoint() is always exactly baseRadius·t away from this, perpendicular to
// the radius here.
inline vec2 involuteTangentPoint(const vec2& center, float baseRadius, float t) {
    return vec2(center.x + baseRadius * std::cos(t), center.y + baseRadius * std::sin(t));
}

// A polyline of `samples + 1` points sampling the involute over the unwinding range [tStart, tEnd].
// `samples` is clamped to at least 1.
inline std::vector<vec2> involutePolyline(const vec2& center, float baseRadius, float tStart, float tEnd,
                                          int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = tStart + (tEnd - tStart) * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(involutePoint(center, baseRadius, t));
    }
    return out;
}

} // namespace maz::math
