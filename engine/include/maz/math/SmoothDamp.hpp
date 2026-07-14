#pragma once

// A critically-damped spring smoother — smoothDamp moves `current` toward
// `target`, tracking `velocity` (an in/out reference the caller persists between
// frames), reaching the target in roughly `smoothTime` seconds without
// overshoot. This is the exact transcription of Unity's Mathf.SmoothDamp,
// including its exponential approximation and overshoot guard, so results are
// deterministic — do NOT substitute a variant. `maxSpeed` caps the approach
// rate. The vec3 overload damps each component independently.
//
// Use for camera/value follow. It complements iter30's Tween: a Tween is a
// fixed-duration curve, whereas smoothDamp is a springy target-follow that
// adapts as the target moves. NOTE: this is NOT a lerp — it is stateful (the
// velocity) and overshoot-free. `deltaTime` must be > 0. Composes maz::math.

#include "maz/math/Math.hpp"

#include "maz/core/Assert.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace maz::math {

// Moves `current` toward `target` like a critically-damped spring, reaching it
// in roughly `smoothTime` seconds without overshoot. `velocity` is in/out — the
// caller persists it between frames (initialize to 0). `smoothTime` is clamped
// internally so 0 is safe (a near-instant snap). `deltaTime` must be > 0.
inline float smoothDamp(float current, float target, float& velocity, float smoothTime,
                        float deltaTime, float maxSpeed = std::numeric_limits<float>::max()) {
    MAZ_ASSERT(deltaTime > 0.0f, "smoothDamp deltaTime must be > 0 (divides by dt in overshoot guard)");
    smoothTime = std::max(0.0001f, smoothTime); // clamp to avoid div-by-zero / instability
    const float omega = 2.0f / smoothTime;
    const float x = omega * deltaTime;
    const float expo = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x); // approx of e^-x
    float change = current - target;
    const float originalTo = target;
    // clamp the maximum change per the max speed
    const float maxChange = maxSpeed * smoothTime;
    change = std::max(-maxChange, std::min(change, maxChange)); // clamp(change, -maxChange, maxChange)
    const float newTarget = current - change;
    const float temp = (velocity + omega * change) * deltaTime;
    velocity = (velocity - omega * temp) * expo;
    float output = newTarget + (change + temp) * expo;
    // prevent overshooting the original target
    if ((originalTo - current > 0.0f) == (output > originalTo)) {
        output = originalTo;
        velocity = (output - originalTo) / deltaTime;
    }
    return output;
}

// Vector overload: damps each component independently (per-axis), reusing the
// float version. This is simpler than Unity's combined-magnitude clamp —
// `maxSpeed` applies per-axis. A magnitude-clamped vector variant is a future
// refinement.
inline vec3 smoothDamp(vec3 current, vec3 target, vec3& velocity, float smoothTime,
                       float deltaTime, float maxSpeed = std::numeric_limits<float>::max()) {
    return vec3(
        smoothDamp(current.x, target.x, velocity.x, smoothTime, deltaTime, maxSpeed),
        smoothDamp(current.y, target.y, velocity.y, smoothTime, deltaTime, maxSpeed),
        smoothDamp(current.z, target.z, velocity.z, smoothTime, deltaTime, maxSpeed));
}

} // namespace maz::math
