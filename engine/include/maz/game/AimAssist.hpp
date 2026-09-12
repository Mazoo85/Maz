#pragma once

#include "maz/math/Math.hpp"      // vec2, dot
#include "maz/math/VectorOps.hpp" // angleTo, rotated

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game aim assist (target magnetism) — the "sticky aim" that makes shooting with a thumbstick feel
// good on a controller: gently rotate the player's aim toward the nearest target that falls inside a
// magnetism CONE, by at most a per-call cap so it nudges rather than snaps. Pick the closest target by
// ANGLE (not distance — what matters is how far the reticle must swing), close a `strength` fraction of
// that gap, and clamp the correction to `maxCorrection` radians so the assist can never yank the aim or
// overshoot the target.
//
// This is deliberately distinct from `game::InterceptAim`, which is a FIRING SOLUTION — where to aim a
// fixed-speed projectile to hit a mover (lead). Aim assist does not lead anything; it is an INPUT-assist
// that biases the player's own aim toward whatever they are roughly pointing at. Godot ships no aim assist.
// Pure vec2 math, header-only, deterministic.
namespace maz::game {

struct AimAssistResult {
    math::vec2 aim{1.0f, 0.0f}; // the adjusted (unit) aim direction
    int target = -1;            // index of the target locked onto, or -1 if none qualified
    float applied = 0.0f;       // signed radians of correction actually applied
};

// Nudge `aimDir` (from `shooter`) toward the closest-by-angle target within `coneHalfAngle` radians.
// `strength` in [0,1] is the fraction of the angular gap closed; `maxCorrection` (radians, >= 0) is a hard
// cap on the rotation. Targets are world positions; ones coincident with the shooter are ignored. If no
// target qualifies (or the aim is degenerate), the aim is returned unchanged with target == -1.
inline AimAssistResult aimAssist(const math::vec2& shooter, const math::vec2& aimDir,
                                 const std::vector<math::vec2>& targets, float coneHalfAngle,
                                 float maxCorrection, float strength = 1.0f) {
    AimAssistResult out;
    const float aimLen2 = math::dot(aimDir, aimDir);
    if (aimLen2 < 1e-12f) return out; // no aim direction to assist
    const math::vec2 na = aimDir / std::sqrt(aimLen2);
    out.aim = na;
    if (coneHalfAngle <= 0.0f || maxCorrection <= 0.0f || strength <= 0.0f) return out;

    int best = -1;
    float bestAbs = coneHalfAngle;   // only targets strictly within the cone qualify
    float bestSigned = 0.0f;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const math::vec2 d = targets[i] - shooter;
        if (math::dot(d, d) < 1e-12f) continue; // target on top of the shooter — no direction
        const float signed_ = math::angleTo(na, d); // signed shortest angle from aim to this target
        const float a = std::fabs(signed_);
        if (a <= bestAbs) {
            bestAbs = a;
            bestSigned = signed_;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) return out; // nothing in the cone

    // Close `strength` of the gap, capped at maxCorrection, never overshooting the target.
    float step = bestAbs * strength;
    if (step > maxCorrection) step = maxCorrection;
    const float applied = (bestSigned >= 0.0f) ? step : -step;
    out.aim = math::rotated(na, applied);
    out.target = best;
    out.applied = applied;
    return out;
}

} // namespace maz::game
