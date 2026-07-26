#pragma once

#include "maz/math/Math.hpp"      // vec2, vec3, dot, cross, normalize
#include "maz/math/MathFuncs.hpp" // isEqualApproxf, isZeroApproxf

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

// True when every component is finite (no NaN, no infinity) — Godot's Vector2/Vector3.is_finite.
// Guards gameplay/physics state after a bad divide or overflow before it spreads.
inline bool isFinite(const vec2& v) { return std::isfinite(v.x) && std::isfinite(v.y); }
inline bool isFinite(const vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Component-wise approximate equality — Godot's Vector2/Vector3.is_equal_approx: each component is
// compared with the scalar relative-epsilon is_equal_approx (CMP_EPSILON = 1e-5), so tiny
// float-rounding differences read as equal.
inline bool isEqualApprox(const vec2& a, const vec2& b) {
    return isEqualApproxf(a.x, b.x) && isEqualApproxf(a.y, b.y);
}
inline bool isEqualApprox(const vec3& a, const vec3& b) {
    return isEqualApproxf(a.x, b.x) && isEqualApproxf(a.y, b.y) && isEqualApproxf(a.z, b.z);
}

// Every component within CMP_EPSILON of zero — Godot's Vector2/Vector3.is_zero_approx.
inline bool isZeroApprox(const vec2& v) { return isZeroApproxf(v.x) && isZeroApproxf(v.y); }
inline bool isZeroApprox(const vec3& v) {
    return isZeroApproxf(v.x) && isZeroApproxf(v.y) && isZeroApproxf(v.z);
}

// Length is 1 within UNIT_EPSILON (0.001) — Godot's Vector2/Vector3.is_normalized. Tested on the
// squared length (no sqrt), and using Godot's exact ABSOLUTE UNIT_EPSILON tolerance, which is looser
// than the relative CMP_EPSILON used for is_equal_approx — matching Godot bit-for-bit.
inline bool isNormalized(const vec2& v) { return std::abs(v.x * v.x + v.y * v.y - 1.0f) < 0.001f; }
inline bool isNormalized(const vec3& v) {
    return std::abs(v.x * v.x + v.y * v.y + v.z * v.z - 1.0f) < 0.001f;
}

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

// Time-parametrised Catmull-Rom — Godot's @GlobalScope.cubic_interpolate_in_time /
// Vector*.cubic_interpolate_in_time. Same four samples as cubicInterpolate, but each neighbour sits
// at its own time (preT < 0 < toT < postT, with `from` anchored at t=0) rather than being evenly
// spaced. This is the Barry-Goldman pyramid of nested lerps, and it reproduces Godot's non-uniform
// animation-keyframe interpolation. When the times are uniform (preT=-1, toT=1, postT=2) it collapses
// back onto the ordinary cubicInterpolate.
template <typename T>
inline T cubicInterpolateInTime(const T& from, const T& to, const T& pre, const T& post, float w,
                                float toT, float preT, float postT) {
    auto lerpT = [](const T& a, const T& b, float t) { return a + (b - a) * t; };
    const float t = toT * w; // lerp(0, toT, w)
    const T a1 = lerpT(pre, from, preT == 0.0f ? 0.0f : (t - preT) / -preT);
    const T a2 = lerpT(from, to, toT == 0.0f ? 0.5f : t / toT);
    const T a3 = lerpT(to, post, (postT - toT) == 0.0f ? 1.0f : (t - toT) / (postT - toT));
    const T b1 = lerpT(a1, a2, (toT - preT) == 0.0f ? 0.0f : (t - preT) / (toT - preT));
    const T b2 = lerpT(a2, a3, postT == 0.0f ? 1.0f : t / postT);
    return lerpT(b1, b2, toT == 0.0f ? 0.5f : t / toT);
}

// Time-parametrised Catmull-Rom between ANGLES, taking the shortest arc across the ±pi wrap —
// Godot's @GlobalScope.cubic_interpolate_angle_in_time. It unwraps `to`/`pre`/`post` to the rotation
// nearest their anchor exactly as cubicInterpolateAngle does, then delegates to cubicInterpolateInTime
// on the unwrapped angles. With uniform times (preT=-1, toT=1, postT=2) it collapses onto
// cubicInterpolateAngle, just as cubicInterpolateInTime collapses onto cubicInterpolate.
inline float cubicInterpolateAngleInTime(float from, float to, float pre, float post, float weight,
                                         float toT, float preT, float postT) {
    const float fromRot = std::fmod(from, kTau);
    const float preDiff = std::fmod(pre - fromRot, kTau);
    const float preRot = fromRot + std::fmod(2.0f * preDiff, kTau) - preDiff;
    const float toDiff = std::fmod(to - fromRot, kTau);
    const float toRot = fromRot + std::fmod(2.0f * toDiff, kTau) - toDiff;
    const float postDiff = std::fmod(post - toRot, kTau);
    const float postRot = toRot + std::fmod(2.0f * postDiff, kTau) - postDiff;
    return cubicInterpolateInTime<float>(fromRot, toRot, preRot, postRot, weight, toT, preT, postT);
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
// Godot's exact spelling for the 2D cross (Vector2.cross), so GDScript-ported `a.cross(b)` maps 1:1.
// GLM defines no vec2 cross, so this overload is unambiguous.
inline float cross(const vec2& a, const vec2& b) { return cross2(a, b); }

// Unit vector pointing at `radians` (measured from +X, CCW): (cos, sin) — Godot's Vector2.from_angle.
// The inverse of angle(): angle(fromAngle(t)) == t. Handy for spawning projectiles / laying out radial
// menus from an angle.
inline vec2 fromAngle(float radians) { return vec2(std::cos(radians), std::sin(radians)); }

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

// Perpendicular vector rotated 90 degrees clockwise — Godot's Vector2.orthogonal (returns (y, -x)).
// Same length as `v`; two applications negate the input (a 180-degree turn).
inline vec2 orthogonal(const vec2& v) { return vec2(v.y, -v.x); }

// Component-wise round / floor / ceil — Godot's Vector2/Vector3.round / floor / ceil. Distinct names
// (roundv/floorv/ceilv) avoid colliding with the scalar C library functions and glm's vector overloads.
// round uses std::round (half away from zero, matching Godot's Math::round). Handy for snapping a
// position to a pixel or tile grid.
inline vec2 roundv(const vec2& v) { return vec2(std::round(v.x), std::round(v.y)); }
inline vec3 roundv(const vec3& v) { return vec3(std::round(v.x), std::round(v.y), std::round(v.z)); }
inline vec2 floorv(const vec2& v) { return vec2(std::floor(v.x), std::floor(v.y)); }
inline vec3 floorv(const vec3& v) { return vec3(std::floor(v.x), std::floor(v.y), std::floor(v.z)); }
inline vec2 ceilv(const vec2& v) { return vec2(std::ceil(v.x), std::ceil(v.y)); }
inline vec3 ceilv(const vec3& v) { return vec3(std::ceil(v.x), std::ceil(v.y), std::ceil(v.z)); }

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

// Rotate the DIRECTION of `from` toward the direction of `to` by at most `maxRadians`, never overshooting;
// `from`'s length is preserved. This is the capped-rate turn a turret, homing missile, or AI uses to face a
// target smoothly — Godot's Vector2.rotate_toward. Unlike `moveToward` (a straight-line translation) it turns
// along an arc, and unlike `rotated` (a fixed angle) it stops exactly on the target heading. A negative
// `maxRadians` turns AWAY. Zero-length `from` or `to` returns `from` unchanged (no direction to rotate).
inline vec2 rotateToward(const vec2& from, const vec2& to, float maxRadians) {
    if (dot(from, from) < 1e-24f || dot(to, to) < 1e-24f) {
        return from;
    }
    const float ang = angleTo(from, to); // signed shortest angle, in (-pi, pi]
    const float step = ang < -maxRadians ? -maxRadians : (ang > maxRadians ? maxRadians : ang);
    return rotated(from, step);
}

// Unit vector pointing from `a` to `b` — Godot's Vector2.direction_to.
inline vec2 directionTo(const vec2& a, const vec2& b) {
    return normalize(b - a);
}

// Squared length (no sqrt) — Godot's Vector2.length_squared. Prefer for comparisons / thresholds.
inline float lengthSquared(const vec2& v) { return v.x * v.x + v.y * v.y; }
// Distance and squared distance between two points — Godot's Vector2.distance_to / distance_squared_to.
inline float distanceSquaredTo(const vec2& a, const vec2& b) { return lengthSquared(b - a); }
inline float distanceTo(const vec2& a, const vec2& b) { return std::sqrt(distanceSquaredTo(a, b)); }
// Component-wise linear interpolation a..b by t (t may leave [0,1]) — Godot's Vector2.lerp.
inline vec2 lerp(const vec2& a, const vec2& b, float t) { return a + (b - a) * t; }

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

// Per-component positive modulo — Godot's Vector2.posmodv: each component wrapped by the matching
// component of `modv` (result carries the modulus component's sign, like fposmod).
inline vec2 posmodv(const vec2& v, const vec2& modv) {
    return vec2(fposmod(v.x, modv.x), fposmod(v.y, modv.y));
}

// Axis index (0=X, 1=Y) of the largest / smallest component — Godot's Vector2.max_axis_index /
// min_axis_index (exact ternary form: max prefers X on ties, min prefers Y).
inline int maxAxisIndex(const vec2& v) { return v.x < v.y ? 1 : 0; }
inline int minAxisIndex(const vec2& v) { return v.x < v.y ? 0 : 1; }

// Snap each component to the nearest multiple of the matching `step` component — Godot's Vector2.snapped.
inline vec2 snapped(const vec2& v, const vec2& step) {
    return vec2(snappedf(v.x, step.x), snappedf(v.y, step.y));
}

// ---- Vector3 --------------------------------------------------------------------------------
// Unsigned angle between `a` and `b` in radians, in [0, pi] — Godot's Vector3.angle_to.
inline float angleTo(const vec3& a, const vec3& b) {
    return std::atan2(length(cross(a, b)), dot(a, b));
}

// Rotate `v` by `radians` about `axis` (which is normalized here) — Godot's Vector3.rotated.
// Right-handed: a positive angle turns counter-clockwise when the axis points toward the viewer.
// Uses Rodrigues' rotation formula: v*cos + (axis x v)*sin + axis*(axis.v)*(1-cos).
inline vec3 rotated(const vec3& v, const vec3& axis, float radians) {
    const vec3 k = normalize(axis);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.0f - c));
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

// Rotate the DIRECTION of `from` toward the direction of `to` by at most `maxRadians`, never overshooting;
// `from`'s length is preserved. The 3D turret / homing / look-at-over-time turn — Godot's Vector3.rotate_toward.
// Turns along the shortest arc (about the axis `from x to`). When the two directions are already within
// `maxRadians` it snaps exactly onto `to`'s heading; a negative `maxRadians` turns away. Antiparallel inputs
// pick an arbitrary perpendicular axis so the turn is still well-defined; a zero-length `from`/`to` returns
// `from` unchanged.
inline vec3 rotateToward(const vec3& from, const vec3& to, float maxRadians) {
    const float fl = std::sqrt(dot(from, from));
    if (fl < 1e-12f || dot(to, to) < 1e-24f) {
        return from;
    }
    const vec3 nf = from / fl;
    const vec3 nt = normalize(to);
    float d = dot(nf, nt);
    d = d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d);
    const float ang = std::acos(d);
    if (std::fabs(maxRadians) >= ang) {
        return nt * fl; // reached (or overshoots) the target heading
    }
    vec3 axis = cross(nf, nt);
    if (dot(axis, axis) < 1e-14f) {
        // Parallel (ang ~ 0, handled above) or antiparallel: choose any perpendicular axis.
        axis = std::fabs(nf.x) < 0.9f ? cross(nf, vec3(1.0f, 0.0f, 0.0f)) : cross(nf, vec3(0.0f, 1.0f, 0.0f));
    }
    return rotated(from, axis, maxRadians);
}

// Squared length / distance / lerp for vec3 — Godot's Vector3.length_squared / distance_to /
// distance_squared_to / lerp (matching the vec2 forms above).
inline float lengthSquared(const vec3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline float distanceSquaredTo(const vec3& a, const vec3& b) { return lengthSquared(b - a); }
inline float distanceTo(const vec3& a, const vec3& b) { return std::sqrt(distanceSquaredTo(a, b)); }
inline vec3 lerp(const vec3& a, const vec3& b, float t) { return a + (b - a) * t; }

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

// Per-component positive modulo — Godot's Vector3.posmodv.
inline vec3 posmodv(const vec3& v, const vec3& modv) {
    return vec3(fposmod(v.x, modv.x), fposmod(v.y, modv.y), fposmod(v.z, modv.z));
}

// Axis index (0=X, 1=Y, 2=Z) of the largest / smallest component — Godot's Vector3.max_axis_index /
// min_axis_index, using Godot's exact nested-ternary form so tie-breaking matches its float type.
inline int maxAxisIndex(const vec3& v) {
    return v.x < v.y ? (v.y < v.z ? 2 : 1) : (v.x < v.z ? 2 : 0);
}
inline int minAxisIndex(const vec3& v) {
    return v.x < v.y ? (v.x < v.z ? 0 : 2) : (v.y < v.z ? 1 : 2);
}

inline vec3 snapped(const vec3& v, const vec3& step) {
    return vec3(snappedf(v.x, step.x), snappedf(v.y, step.y), snappedf(v.z, step.z));
}

// ---- Godot 4.x scalar-bound component ops -----------------------------------------------------
// These take a plain float and apply it to every component. They use DISTINCT names (clampf / minf /
// maxf / snappedf-of-vec) rather than clamp/min/max, deliberately, so they never collide with GLM's
// vector clamp/min/max overloads under ADL. The vec2-vs-scalar first argument disambiguates the
// snappedf overloads from the scalar snappedf above.

// Clamp every component into [minv, maxv] — Godot's Vector2/Vector3.clampf.
inline vec2 clampf(const vec2& v, float minv, float maxv) {
    return vec2(v.x < minv ? minv : (v.x > maxv ? maxv : v.x),
                v.y < minv ? minv : (v.y > maxv ? maxv : v.y));
}
inline vec3 clampf(const vec3& v, float minv, float maxv) {
    return vec3(v.x < minv ? minv : (v.x > maxv ? maxv : v.x),
                v.y < minv ? minv : (v.y > maxv ? maxv : v.y),
                v.z < minv ? minv : (v.z > maxv ? maxv : v.z));
}

// Component-wise minimum / maximum against a scalar — Godot's Vector2/Vector3.minf / maxf.
inline vec2 minf(const vec2& v, float s) { return vec2(v.x < s ? v.x : s, v.y < s ? v.y : s); }
inline vec3 minf(const vec3& v, float s) {
    return vec3(v.x < s ? v.x : s, v.y < s ? v.y : s, v.z < s ? v.z : s);
}
inline vec2 maxf(const vec2& v, float s) { return vec2(v.x > s ? v.x : s, v.y > s ? v.y : s); }
inline vec3 maxf(const vec3& v, float s) {
    return vec3(v.x > s ? v.x : s, v.y > s ? v.y : s, v.z > s ? v.z : s);
}

// Snap every component to the nearest multiple of a single scalar step — Godot's snappedf(float).
inline vec2 snappedf(const vec2& v, float step) {
    return vec2(snappedf(v.x, step), snappedf(v.y, step));
}
inline vec3 snappedf(const vec3& v, float step) {
    return vec3(snappedf(v.x, step), snappedf(v.y, step), snappedf(v.z, step));
}

// Octahedral encoding of a UNIT vector into a vec2 in [0,1]^2 — Godot's Vector3.octahedron_encode.
// Packs surface normals to two channels for G-buffers / compressed vertex data; pair with
// octahedronDecode. Assumes the input is normalized.
inline vec2 octahedronEncode(const vec3& n) {
    const vec3 an = n / (std::fabs(n.x) + std::fabs(n.y) + std::fabs(n.z));
    vec2 o;
    if (an.z >= 0.0f) {
        o = vec2(an.x, an.y);
    } else {
        o.x = (1.0f - std::fabs(an.y)) * (an.x >= 0.0f ? 1.0f : -1.0f);
        o.y = (1.0f - std::fabs(an.x)) * (an.y >= 0.0f ? 1.0f : -1.0f);
    }
    return o * 0.5f + 0.5f;
}

// Inverse of octahedronEncode: recover the unit vector from its [0,1]^2 encoding — Godot's
// Vector3.octahedron_decode.
inline vec3 octahedronDecode(const vec2& oct) {
    const vec2 f(oct.x * 2.0f - 1.0f, oct.y * 2.0f - 1.0f);
    vec3 n(f.x, f.y, 1.0f - std::fabs(f.x) - std::fabs(f.y));
    const float t = n.z < 0.0f ? -n.z : 0.0f; // clamp(-n.z, 0, 1); n.z can't exceed 1 here
    n.x += n.x >= 0.0f ? -t : t;
    n.y += n.y >= 0.0f ? -t : t;
    return normalize(n);
}

// Spherical interpolation of `a` toward `to` by t — Godot's Vector2.slerp: rotates along the arc
// between the two directions while linearly interpolating length. Falls back to a straight lerp when
// either vector has zero length (a direction can't be defined).
inline vec2 slerp(const vec2& a, const vec2& to, float t) {
    const float startLenSq = dot(a, a);
    const float endLenSq = dot(to, to);
    if (startLenSq == 0.0f || endLenSq == 0.0f) {
        return a + (to - a) * t;
    }
    const float startLen = std::sqrt(startLenSq);
    const float resultLen = startLen + (std::sqrt(endLenSq) - startLen) * t;
    const float ang = angleTo(a, to);
    return rotated(a, ang * t) * (resultLen / startLen);
}

// Spherical interpolation of `a` toward `to` by t — Godot's Vector3.slerp. Rotates about the axis
// perpendicular to both vectors (Rodrigues) while lerping length. Falls back to a straight lerp when
// either vector is zero-length or the two are colinear (no rotation axis).
inline vec3 slerp(const vec3& a, const vec3& to, float t) {
    const float startLenSq = dot(a, a);
    const float endLenSq = dot(to, to);
    if (startLenSq == 0.0f || endLenSq == 0.0f) {
        return a + (to - a) * t;
    }
    vec3 axis = cross(a, to);
    const float axisLenSq = dot(axis, axis);
    if (axisLenSq == 0.0f) {
        return a + (to - a) * t; // colinear -> no rotation axis
    }
    axis /= std::sqrt(axisLenSq);
    const float startLen = std::sqrt(startLenSq);
    const float resultLen = startLen + (std::sqrt(endLenSq) - startLen) * t;
    const float ang = angleTo(a, to) * t;
    // Rodrigues' rotation of `a` about the unit `axis` by `ang`.
    const float c = std::cos(ang), s = std::sin(ang);
    const vec3 rotatedA = a * c + cross(axis, a) * s + axis * (dot(axis, a) * (1.0f - c));
    return rotatedA * (resultLen / startLen);
}

} // namespace maz::math
