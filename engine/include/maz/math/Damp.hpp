#pragma once

#include "maz/math/Math.hpp"      // vec2, vec3
#include "maz/math/MathFuncs.hpp" // angleDifference

#include <cmath>

// maz::math frame-rate-independent exponential smoothing — the correct way to make a value "chase" a
// target a little bit each frame. The tempting one-liner `x = lerp(x, target, 0.1f)` is the classic
// game-code bug: its speed depends on the frame rate, so the same code eases faster at 120 fps than at
// 30 fps and behaves differently on every machine. `damp` fixes that by using the exact closed-form
// solution of exponential decay, x(t) = target + (x0 - target) * 2^(-dt / halfLife), which composes
// perfectly across sub-steps: one step of dt gives BIT-FOR-BIT the same result as two steps of dt/2.
//
// The knob is the HALF-LIFE: how long (in seconds) it takes to close half the remaining gap — an
// intuitive, designer-facing number ("snappy" ~0.05s, "floaty" ~0.5s). This is deliberately different
// from the engine's other smoothers: core::smoothDamp is a critically-damped spring that needs a
// persistent velocity and a max-speed clamp (Unity's SmoothDamp), core::Spring is a full physical
// spring, and math::moveToward / rotateToward move at a CONSTANT rate. `damp` is the stateless,
// unconditionally-stable, overshoot-free exponential ease that camera/UI/audio code reaches for most
// often — and which the engine previously open-coded inline (e.g. CameraController2D). Pure, header-only,
// deterministic.
namespace maz::math {

// Exponential decay factor 2^(-dt / halfLife) — the fraction of the CURRENT gap that survives after
// time dt. In [0,1]: 1 keeps the whole gap (no movement), 0 snaps fully to target. A halfLife <= 0
// means "snap instantly" (factor 0); a dt <= 0 means "no time passed" (factor 1). Reuse this when
// smoothing several values with the same half-life so the exp2 is paid once.
inline float dampFactor(float halfLife, float dt) {
    if (halfLife <= 0.0f) return 0.0f; // instant snap
    if (dt <= 0.0f) return 1.0f;       // no time elapsed, gap unchanged
    return std::exp2(-dt / halfLife);
}

// Move `current` toward `target` by frame-rate-independent exponential decay over dt seconds.
// `halfLife` is the time to close half the gap. Always stable and never overshoots, for any dt.
inline float damp(float current, float target, float halfLife, float dt) {
    const float k = dampFactor(halfLife, dt);
    return target + (current - target) * k;
}

// Vector overloads (component-wise, sharing one decay factor).
inline vec2 damp(const vec2& current, const vec2& target, float halfLife, float dt) {
    const float k = dampFactor(halfLife, dt);
    return target + (current - target) * k;
}

inline vec3 damp(const vec3& current, const vec3& target, float halfLife, float dt) {
    const float k = dampFactor(halfLife, dt);
    return target + (current - target) * k;
}

// Angular version: smooth `current` toward `target` the SHORT way around the circle (so easing from
// 350deg to 10deg crosses 0 rather than winding backwards through 180). Both in radians; the result is
// not wrapped (matches the engine's rotateToward / Godot's lerp_angle convention).
inline float dampAngle(float current, float target, float halfLife, float dt) {
    const float k = dampFactor(halfLife, dt);
    return current + angleDifference(current, target) * (1.0f - k);
}

} // namespace maz::math
