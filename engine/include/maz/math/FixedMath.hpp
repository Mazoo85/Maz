#pragma once

#include "maz/core/Fixed.hpp"
#include "maz/math/FixedVec2.hpp"
#include "maz/math/FixedTrig.hpp"

// maz::math::FixedMath — the everyday DETERMINISTIC helpers for core::Fixed and FixedVec2: the
// clamp / lerp / move-toward / rotate operations gameplay leans on constantly, but done in pure
// integer fixed-point so they give bit-identical results on every machine (lockstep multiplayer,
// replays, cross-platform play). These are the fixed-point twins of Godot's float lerp/clamp/
// move_toward and Vector2.rotated / from_angle / limit_length — none of which Godot offers in a
// deterministic form. Built on M415 Fixed (scalar), M416 FixedVec2 (vector) and M417 FixedTrig
// (integer CORDIC sin/cos). Header-only, pure, no floats on the runtime path.
namespace maz::math {

// ---- Scalar Fixed helpers. ----

inline constexpr Fixed fixMin(Fixed a, Fixed b) { return a < b ? a : b; }
inline constexpr Fixed fixMax(Fixed a, Fixed b) { return a > b ? a : b; }

// Clamp v into [lo, hi]. If lo > hi the result is lo (matches "clamp to the low bound" convention).
inline constexpr Fixed fixClamp(Fixed v, Fixed lo, Fixed hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// -1 / 0 / +1 as a Fixed, by sign of v.
inline constexpr Fixed fixSign(Fixed v) {
    return v.raw > 0 ? Fixed::one() : (v.raw < 0 ? -Fixed::one() : Fixed::zero());
}

// Linear interpolation a + (b - a) * t. Exact at t = 0 (=a) and t = 1 (=b); t is not clamped.
inline Fixed fixLerp(Fixed a, Fixed b, Fixed t) { return a + (b - a) * t; }

// Step `from` toward `to` by at most `delta` (delta >= 0), never overshooting — Godot's move_toward.
inline Fixed fixMoveToward(Fixed from, Fixed to, Fixed delta) {
    const Fixed diff = to - from;
    if (diff.abs() <= delta) {
        return to;
    }
    return from + fixSign(diff) * delta;
}

// ---- FixedVec2 helpers. ----

// Component-wise lerp; exact at t = 0 and t = 1.
inline FixedVec2 fixLerp(FixedVec2 a, FixedVec2 b, Fixed t) {
    return FixedVec2(fixLerp(a.x, b.x, t), fixLerp(a.y, b.y, t));
}

// Move `from` toward `to` by at most `delta` (delta >= 0), never overshooting — Vector2.move_toward.
inline FixedVec2 fixMoveToward(FixedVec2 from, FixedVec2 to, Fixed delta) {
    const FixedVec2 diff = to - from;
    const Fixed dist = diff.length();
    if (dist == Fixed::zero() || dist <= delta) {
        return to;
    }
    // from + normalize(diff) * delta, done as diff * (delta / dist) to keep one division.
    return from + diff * (delta / dist);
}

// Rotate a vector by `angle` radians (counter-clockwise), deterministically via the CORDIC sin/cos.
inline FixedVec2 fixRotated(FixedVec2 v, Fixed angle) {
    const FixSinCos sc = fixSinCos(angle);
    return FixedVec2(v.x * sc.cos - v.y * sc.sin, v.x * sc.sin + v.y * sc.cos);
}

// The unit vector (cos a, sin a) scaled by `length` — Godot's Vector2.from_angle * length.
inline FixedVec2 fixFromAngle(Fixed angle, Fixed length = Fixed::one()) {
    const FixSinCos sc = fixSinCos(angle);
    return FixedVec2(sc.cos * length, sc.sin * length);
}

// Shorten `v` so its length is at most `maxLength` (>= 0); shorter vectors pass through unchanged.
inline FixedVec2 fixClampLength(FixedVec2 v, Fixed maxLength) {
    const Fixed len = v.length();
    if (len == Fixed::zero() || len <= maxLength) {
        return v;
    }
    return v * (maxLength / len);
}

// The angle of `v` in radians (-pi, pi], deterministically — Godot's Vector2.angle. Inverse of
// fixFromAngle: fixFromAngle(fixAngle(v)) points the same way as v.
inline Fixed fixAngle(FixedVec2 v) { return fixAtan2(v.y, v.x); }

// The signed angle from `from` to `to` (radians), the deterministic Vector2.angle_to.
inline Fixed fixAngleTo(FixedVec2 from, FixedVec2 to) {
    // atan2(cross, dot) gives the shortest signed turn between the two directions.
    return fixAtan2(from.cross(to), from.dot(to));
}

// The shortest signed difference between two angles (radians), wrapped into [-pi, pi] — the
// deterministic twin of Godot's angle_difference. Handy for turning toward a heading.
inline Fixed fixAngleDifference(Fixed from, Fixed to) {
    Fixed d = to - from;
    // Wrap into (-pi, pi] by adding/subtracting whole turns in the fixed representation.
    while (d.raw > fixPi().raw) {
        d = d - fixTwoPi();
    }
    while (d.raw <= -fixPi().raw) {
        d = d + fixTwoPi();
    }
    return d;
}

} // namespace maz::math
