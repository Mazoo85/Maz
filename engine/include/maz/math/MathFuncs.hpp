#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// maz::math scalar math helpers — Godot's @GlobalScope numeric utilities that gameplay code reaches for
// constantly but the C++ standard library does not spell: remap / inverse_lerp, wrapf / wrapi (looping
// values), smoothstep, ease (Godot's tunable easing curve), lerp_angle (shortest-arc angle blend),
// pingpong, nearest_po2, deg<->rad, and approximate comparisons. Semantics match Godot's exactly
// (verified against its source). Header-only, pure, unit-tested. (Vector variants of move_toward /
// snapped live in VectorOps; this is the scalar layer.)
namespace maz::math {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTau = 6.28318530717958647692f;

inline float degToRad(float deg) { return deg * (kPi / 180.0f); }
inline float radToDeg(float rad) { return rad * (180.0f / kPi); }

inline float signf(float x) { return static_cast<float>((x > 0.0f) - (x < 0.0f)); }
inline int signi(std::int64_t x) { return (x > 0) - (x < 0); }

// Linear interpolate a..b by t (t may go outside [0,1]) — Godot's @GlobalScope lerp.
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Inverse of lerp: where does `v` sit between a and b? — Godot's inverse_lerp.
inline float inverseLerp(float a, float b, float v) {
    return (b - a) != 0.0f ? (v - a) / (b - a) : 0.0f;
}

// Map `v` from input range [iMin,iMax] to output range [oMin,oMax] — Godot's remap.
inline float remap(float v, float iMin, float iMax, float oMin, float oMax) {
    return lerpf(oMin, oMax, inverseLerp(iMin, iMax, v));
}

// Wrap a float into [min, max) (looping), carrying the sign of the range — Godot's wrapf.
inline float wrapf(float value, float minv, float maxv) {
    const float range = maxv - minv;
    if (std::abs(range) < 1e-8f) {
        return minv;
    }
    return value - range * std::floor((value - minv) / range);
}

// Wrap an integer into [min, max) (looping) — Godot's wrapi.
inline std::int64_t wrapi(std::int64_t value, std::int64_t minv, std::int64_t maxv) {
    const std::int64_t range = maxv - minv;
    if (range == 0) {
        return minv;
    }
    return minv + ((value - minv) % range + range) % range;
}

// Hermite smoothstep: 0 below `from`, 1 above `to`, smooth in between — Godot's smoothstep.
inline float smoothstep(float from, float to, float x) {
    if (from == to) {
        return x < from ? 0.0f : 1.0f;
    }
    float s = (x - from) / (to - from);
    s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
    return s * s * (3.0f - 2.0f * s);
}

// Godot's ease(): shape a 0..1 value with a curve param. c>1 ease-in, 0<c<1 ease-out, c<0 in-out.
inline float ease(float x, float c) {
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    if (c > 0.0f) {
        if (c < 1.0f) {
            return 1.0f - std::pow(1.0f - x, 1.0f / c);
        }
        return std::pow(x, c);
    }
    if (c < 0.0f) {
        if (x < 0.5f) {
            return std::pow(x * 2.0f, -c) * 0.5f;
        }
        return (1.0f - std::pow(1.0f - (x - 0.5f) * 2.0f, -c)) * 0.5f + 0.5f;
    }
    return 0.0f;
}

// Step `from` toward `to` by at most `delta` (no overshoot) — Godot's scalar move_toward.
inline float moveTowardf(float from, float to, float delta) {
    return std::abs(to - from) <= delta ? to : from + signf(to - from) * delta;
}

// Blend angle `from` toward `to` by `weight` along the SHORTEST arc — Godot's lerp_angle.
inline float lerpAngle(float from, float to, float weight) {
    const float diff = std::fmod(to - from, kTau);
    const float dist = std::fmod(2.0f * diff, kTau) - diff;
    return from + dist * weight;
}

// Catmull-Rom cubic between two ANGLES, taking the shortest arc across the ±pi wrap — Godot's
// @GlobalScope.cubic_interpolate_angle. Each of `to`/`pre`/`post` is first remapped to the rotation
// nearest the anchor (Godot's exact fmod construction), then a plain cubic runs on the unwrapped
// angles, so interpolating e.g. from 350deg to 10deg goes the short way (forward through 0).
inline float cubicInterpolateAngle(float from, float to, float pre, float post, float weight) {
    const float fromRot = std::fmod(from, kTau);
    const float preDiff = std::fmod(pre - fromRot, kTau);
    const float preRot = fromRot + std::fmod(2.0f * preDiff, kTau) - preDiff;
    const float toDiff = std::fmod(to - fromRot, kTau);
    const float toRot = fromRot + std::fmod(2.0f * toDiff, kTau) - toDiff;
    const float postDiff = std::fmod(post - toRot, kTau);
    const float postRot = toRot + std::fmod(2.0f * postDiff, kTau) - postDiff;
    const float w2 = weight * weight;
    const float w3 = w2 * weight;
    return 0.5f * ((fromRot * 2.0f) + (toRot - preRot) * weight +
                   (preRot * 2.0f - fromRot * 5.0f + toRot * 4.0f - postRot) * w2 +
                   (-preRot + fromRot * 3.0f - toRot * 3.0f + postRot) * w3);
}

// Triangle wave: ramps 0->length->0 as `t` increases — Godot's pingpong.
inline float pingpong(float t, float length) {
    if (length == 0.0f) {
        return 0.0f;
    }
    const float denom = length * 2.0f;
    float frac = (t - length) / denom;
    frac = frac - std::floor(frac); // fract
    return std::abs(frac * denom - length);
}

// Snap `value` to the nearest multiple of `step` (integer) — Godot's snappedi.
inline std::int64_t snappedi(double value, std::int64_t step) {
    if (step == 0) {
        return static_cast<std::int64_t>(std::llround(value));
    }
    return static_cast<std::int64_t>(std::llround(value / static_cast<double>(step))) * step;
}

// Smallest power of two >= value (Godot's nearest_po2). Returns 0 for value <= 0; 1 for value == 1.
inline std::uint64_t nearestPo2(std::int64_t value) {
    if (value <= 0) {
        return 0;
    }
    std::uint64_t v = static_cast<std::uint64_t>(value) - 1u;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return v + 1u;
}

// Relative-epsilon float comparison — Godot's is_equal_approx.
inline bool isEqualApproxf(float a, float b, float eps = 1e-5f) {
    if (a == b) {
        return true;
    }
    float tol = eps * std::abs(a);
    if (tol < eps) {
        tol = eps;
    }
    return std::abs(a - b) < tol;
}
inline bool isZeroApproxf(float a, float eps = 1e-5f) { return std::abs(a) < eps; }

// Shortest signed angular distance from `from` to `to`, in radians, wrapped to (-pi, pi] — Godot's
// @GlobalScope.angle_difference (4.2+). Positive means `to` is counter-clockwise of `from`.
inline float angleDifference(float from, float to) {
    const float diff = std::fmod(to - from, kTau);
    return std::fmod(2.0f * diff, kTau) - diff;
}

// Rotate angle `from` toward `to` by at most `delta` radians, taking the short way and never
// overshooting — Godot's @GlobalScope.rotate_toward (4.2+). A negative `delta` rotates away from
// `to` (down to the opposite angle); the result is not wrapped (matches Godot).
inline float rotateToward(float from, float to, float delta) {
    const float diff = angleDifference(from, to);
    const float absDiff = std::abs(diff);
    return from + std::clamp(delta, absDiff - kPi, absDiff) * (diff >= 0.0f ? 1.0f : -1.0f);
}

} // namespace maz::math
