#pragma once

#include "maz/math/Math.hpp" // vec2

namespace maz::math {

// maz::math Kochanek-Bartels (TCB) spline — the interpolating keyframe spline with artist controls.
//
// The engine already has centripetal Catmull-Rom (CatmullRomSpline) and the uniform/cubic B-spline
// (BSpline). Kochanek-Bartels is the animation industry's keyframe spline (3ds Max, Maya, classic game
// tools): like Catmull-Rom it passes THROUGH every keyframe, but each keyframe carries three knobs an
// animator dials in per key:
//   * Tension    — how sharply the curve bends through the key (1 = taut/linear, -1 = slack/round).
//   * Continuity — how smoothly it passes through (0 = smooth; ±1 introduces a corner / "snap").
//   * Bias       — which side the curve leans toward (+1 = overshoots past, -1 = undershoots before).
// With Tension=Continuity=Bias=0 it is EXACTLY uniform Catmull-Rom — which is the exact cross-check the
// tests use. It works by deriving an incoming and an outgoing tangent at each key from those three knobs,
// then Hermite-interpolating each segment. Pure vec2 math, header-only, deterministic.
struct TcbParams {
    float tension = 0.0f;    // t in [-1,1]
    float continuity = 0.0f; // c in [-1,1]
    float bias = 0.0f;       // b in [-1,1]
};

// Outgoing tangent at `curr` (leaving toward `next`), from its neighbours and the TCB knobs.
inline vec2 tcbTangentOut(const vec2& prev, const vec2& curr, const vec2& next, const TcbParams& p) {
    const float a = (1.0f - p.tension) * (1.0f + p.continuity) * (1.0f + p.bias) * 0.5f;
    const float b = (1.0f - p.tension) * (1.0f - p.continuity) * (1.0f - p.bias) * 0.5f;
    return vec2(a * (curr.x - prev.x) + b * (next.x - curr.x),
                a * (curr.y - prev.y) + b * (next.y - curr.y));
}

// Incoming tangent at `curr` (arriving from `prev`), from its neighbours and the TCB knobs.
inline vec2 tcbTangentIn(const vec2& prev, const vec2& curr, const vec2& next, const TcbParams& p) {
    const float a = (1.0f - p.tension) * (1.0f - p.continuity) * (1.0f + p.bias) * 0.5f;
    const float b = (1.0f - p.tension) * (1.0f + p.continuity) * (1.0f - p.bias) * 0.5f;
    return vec2(a * (curr.x - prev.x) + b * (next.x - curr.x),
                a * (curr.y - prev.y) + b * (next.y - curr.y));
}

// Cubic Hermite: point on the segment from p0 (tangent m0) to p1 (tangent m1) at s in [0,1].
inline vec2 hermite(const vec2& p0, const vec2& m0, const vec2& p1, const vec2& m1, float s) {
    const float s2 = s * s;
    const float s3 = s2 * s;
    const float h00 = 2.0f * s3 - 3.0f * s2 + 1.0f;
    const float h10 = s3 - 2.0f * s2 + s;
    const float h01 = -2.0f * s3 + 3.0f * s2;
    const float h11 = s3 - s2;
    return vec2(h00 * p0.x + h10 * m0.x + h01 * p1.x + h11 * m1.x,
                h00 * p0.y + h10 * m0.y + h01 * p1.y + h11 * m1.y);
}

// Evaluate the TCB segment on [p1, p2], using outer neighbours p0 (before p1) and p3 (after p2) to
// shape the tangents. s in [0,1]; hits p1 at s=0 and p2 at s=1. With the default (all-zero) params this
// is uniform Catmull-Rom.
inline vec2 tcbSegment(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float s,
                       const TcbParams& params = TcbParams{}) {
    const vec2 m0 = tcbTangentOut(p0, p1, p2, params); // leaving p1
    const vec2 m1 = tcbTangentIn(p1, p2, p3, params);  // arriving at p2
    return hermite(p1, m0, p2, m1, s);
}

} // namespace maz::math
