#pragma once

#include "maz/math/Math.hpp" // vec2, vec3, dot, cross, normalize

#include <cmath>

// maz::math vector helpers — the everyday Vector2 / Vector3 methods Godot game code reaches for
// constantly but that raw GLM does not spell the same way: move_toward, slide / bounce / reflect
// (wall sliding + projectile ricochet), limit_length, direction_to, angle_to, posmod, snapped,
// rotated, project, and component-wise lerp / clamp. Each mirrors Godot's exact semantics (verified
// against Godot's Vector2/Vector3 source), so gameplay logic ported from GDScript behaves
// identically. Header-only, pure, branch-for-branch deterministic — unit-tested to the bit.
//
// A note on reflect vs bounce, because Godot's convention trips people up: Godot's `reflect(n)`
// mirrors the vector ABOUT the normal direction (2*n*(v·n) - v), while `bounce(n)` is the physical
// "ricochet off a surface with normal n" (v - 2*n*(v·n)) — i.e. bounce == -reflect. We keep Godot's
// spelling so ported code matches. All functions taking a normal `n` assume it is unit length.
namespace maz::math {

// ---- shared scalar helpers ------------------------------------------------------------------
// Positive modulo: result carries the sign of `y` (Godot's @GlobalScope.posmod / fposmod), so
// posmod(-1, 3) == 2, unlike C++ std::fmod(-1, 3) == -1. Used component-wise by the vector posmod.
inline float fposmod(float x, float y) {
    float m = std::fmod(x, y);
    if (m != 0.0f && ((y < 0.0f) != (m < 0.0f))) {
        m += y;
    }
    return m;
}

// Snap a scalar to the nearest multiple of `step` (Godot's snappedf). step == 0 leaves x unchanged.
inline float snappedf(float x, float step) {
    if (step != 0.0f) {
        return std::floor(x / step + 0.5f) * step;
    }
    return x;
}

// ---- interpolation (Godot's cubic / bezier interpolate, scalar + vector) (M279) --------------
// These are templated so they work for float, vec2, and vec3 alike (all support + and scalar *).

// Catmull-Rom cubic interpolation from `from` to `to` guided by the neighbouring `pre`/`post`
// samples — Godot's @GlobalScope.cubic_interpolate / Vector2.cubic_interpolate.
template <typename T>
inline T cubicInterpolate(const T& from, const T& to, const T& pre, const T& post, float w) {
    const float w2 = w * w;
    const float w3 = w2 * w;
    return (from * 2.0f + (to + pre * -1.0f) * w +
            (pre * 2.0f - from * 5.0f + to * 4.0f - post) * w2 +
            (pre * -1.0f + from * 3.0f - to * 3.0f + post) * w3) *
           0.5f;
}

// Cubic Bézier interpolation with two control points — Godot's Vector2.bezier_interpolate.
template <typename T>
inline T bezierInterpolate(const T& start, const T& c1, const T& c2, const T& end, float t) {
    const float omt = 1.0f - t;
    const float omt2 = omt * omt;
    const float omt3 = omt2 * omt;
    const float t2 = t * t;
    const float t3 = t2 * t;
    return start * omt3 + c1 * (omt2 * t * 3.0f) + c2 * (omt * t2 * 3.0f) + end * t3;
}

// Derivative (tangent) of the cubic Bézier at `t` — Godot's Vector2.bezier_derivative.
template <typename T>
inline T bezierDerivative(const T& start, const T& c1, const T& c2, const T& end, float t) {
    const float omt = 1.0f - t;
    const float omt2 = omt * omt;
    const float t2 = t * t;
    return (c1 - start) * (omt2 * 3.0f) + (c2 - c1) * (omt * t * 6.0f) + (end - c2) * (t2 * 3.0f);
}

// ---- Vector2 --------------------------------------------------------------------------------
// 2D scalar cross product (the z of the 3D cross): positive when `b` is counter-clockwise from `a`.
inline float cross2(const vec2& a, const vec2& b) {
    return a.x * b.y - a.y * b.x;
}

// Orientation angle of the vector, atan2(y, x) — Godot's Vector2.angle().
inline float angle(const vec2& v) {
    return std::atan2(v.y, v.x);
}

// Signed angle from `a` to `b` in radians, in (-pi, pi] — Godot's Vector2.angle_to.
inline float angleTo(const vec2& a, const vec2& b) {
    return std::atan2(cross2(a, b), dot(a, b));
}

// Angle of the line from `a` toward point `b` — Godot's Vector2.angle_to_point.
inline float angleToPoint(const vec2& a, const vec2& b) {
    return angle(b - a);
}

// Rotate `v` by `radians` counter-clockwise — Godot's Vector2.rotated.
inline vec2 rotated(const vec2& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// Ratio width/height — Godot's Vector2.aspect().
inline float aspect(const vec2& v) {
    return v.x / v.y;
}

// Move `from` toward `to` by at most `delta`, never overshooting — Godot's Vector2.move_toward.
inline vec2 moveToward(const vec2& from, const vec2& to, float delta) {
    const vec2 d = to - from;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len <= delta || len < 1e-6f) {
        return to;
    }
    return from + d / len * delta;
}

// Clamp the vector's length to `maxLen` (direction preserved) — Godot's Vector2.limit_length.
inline vec2 limitLength(const vec2& v, float maxLen = 1.0f) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y);
    if (l > 0.0f && maxLen < l) {
        return v / l * maxLen;
    }
    return v;
}

// Unit vector pointing from `a` to `b` — Godot's Vector2.direction_to.
inline vec2 directionTo(const vec2& a, const vec2& b) {
    return normalize(b - a);
}

// Component of `v` perpendicular to unit normal `n` (slide along the surface) — Godot's Vector2.slide.
inline vec2 slide(const vec2& v, const vec2& n) {
    return v - n * dot(v, n);
}

// Mirror `v` about the direction `n` — Godot's Vector2.reflect (note: NOT the physics ricochet).
inline vec2 reflect(const vec2& v, const vec2& n) {
    return 2.0f * n * dot(v, n) - v;
}

// Ricochet `v` off a surface with unit normal `n` (== -reflect) — Godot's Vector2.bounce.
inline vec2 bounce(const vec2& v, const vec2& n) {
    return v - 2.0f * n * dot(v, n);
}

// Projection of `v` onto `b` — Godot's Vector2.project.
inline vec2 project(const vec2& v, const vec2& b) {
    return b * (dot(v, b) / dot(b, b));
}

// Component-wise positive modulo — Godot's Vector2.posmod (scalar) semantics.
inline vec2 posmod(const vec2& v, float m) {
    return vec2(fposmod(v.x, m), fposmod(v.y, m));
}

// Snap each component to the nearest multiple of the matching `step` component — Godot's Vector2.snapped.
inline vec2 snapped(const vec2& v, const vec2& step) {
    return vec2(snappedf(v.x, step.x), snappedf(v.y, step.y));
}

// ---- Vector3 --------------------------------------------------------------------------------
// Unsigned angle between `a` and `b` in radians, in [0, pi] — Godot's Vector3.angle_to.
inline float angleTo(const vec3& a, const vec3& b) {
    return std::atan2(length(cross(a, b)), dot(a, b));
}

inline vec3 moveToward(const vec3& from, const vec3& to, float delta) {
    const vec3 d = to - from;
    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len <= delta || len < 1e-6f) {
        return to;
    }
    return from + d / len * delta;
}

inline vec3 limitLength(const vec3& v, float maxLen = 1.0f) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l > 0.0f && maxLen < l) {
        return v / l * maxLen;
    }
    return v;
}

inline vec3 directionTo(const vec3& a, const vec3& b) {
    return normalize(b - a);
}

inline vec3 slide(const vec3& v, const vec3& n) {
    return v - n * dot(v, n);
}

inline vec3 reflect(const vec3& v, const vec3& n) {
    return 2.0f * n * dot(v, n) - v;
}

inline vec3 bounce(const vec3& v, const vec3& n) {
    return v - 2.0f * n * dot(v, n);
}

inline vec3 project(const vec3& v, const vec3& b) {
    return b * (dot(v, b) / dot(b, b));
}

inline vec3 posmod(const vec3& v, float m) {
    return vec3(fposmod(v.x, m), fposmod(v.y, m), fposmod(v.z, m));
}

inline vec3 snapped(const vec3& v, const vec3& step) {
    return vec3(snappedf(v.x, step.x), snappedf(v.y, step.y), snappedf(v.z, step.z));
}

} // namespace maz::math
