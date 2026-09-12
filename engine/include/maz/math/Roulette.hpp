#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math roulette curves — the family of curves traced by a point attached to a circle that ROLLS,
// either along a straight line (trochoid / cycloid) or around another circle (epi-/hypo-trochoid, i.e. the
// classic "Spirograph" curves). These give you cardioids, astroids, deltoids, gear-tooth flanks, cycloidal
// gear profiles, spirograph rosettes and rolling-wheel motion paths from a couple of numbers — none of which
// Godot offers as a primitive. Conventions: `r` is the rolling circle's radius, `R` the fixed circle's
// radius, `d` the distance of the traced point from the rolling circle's centre (d = r → the point is on
// the rim, giving a cycloid/epicycloid/hypocycloid; d < r "curtate", d > r "prolate"), and `t` the roll
// angle in radians. Header-only, std-only, deterministic.
namespace maz::math {

// --- Rolling along a straight line (the x-axis). ---

// General trochoid: the curve traced by a point at distance `d` from the centre of a circle of radius `r`
// rolling along the x-axis. d = r gives the cycloid; d < r a (smooth) curtate trochoid; d > r a looping
// prolate trochoid.
inline vec2 trochoidPoint(float r, float d, float t) {
    return vec2(r * t - d * std::sin(t), r - d * std::cos(t));
}

// Cycloid: the arch traced by a point on the rim of a rolling wheel (trochoid with d = r).
inline vec2 cycloidPoint(float r, float t) {
    return trochoidPoint(r, r, t);
}

// --- Rolling on the OUTSIDE of a fixed circle of radius R (epitrochoid / epicycloid). ---

// Epitrochoid: point at distance `d` from the centre of a circle of radius `r` rolling around the outside of
// a fixed circle of radius `R`, centred at the origin.
inline vec2 epitrochoidPoint(float R, float r, float d, float t) {
    const float k = (R + r) / r;
    return vec2((R + r) * std::cos(t) - d * std::cos(k * t),
                (R + r) * std::sin(t) - d * std::sin(k * t));
}

// Epicycloid: the rim point (d = r). R = r gives a cardioid; R = 2r a nephroid; R = n·r an n-cusped rosette.
inline vec2 epicycloidPoint(float R, float r, float t) {
    return epitrochoidPoint(R, r, r, t);
}

// --- Rolling on the INSIDE of a fixed circle of radius R (hypotrochoid / hypocycloid). ---

// Hypotrochoid: point at distance `d` from the centre of a circle of radius `r` rolling around the inside of
// a fixed circle of radius `R`, centred at the origin. This is the classic Spirograph curve.
inline vec2 hypotrochoidPoint(float R, float r, float d, float t) {
    const float k = (R - r) / r;
    return vec2((R - r) * std::cos(t) + d * std::cos(k * t),
                (R - r) * std::sin(t) - d * std::sin(k * t));
}

// Hypocycloid: the rim point (d = r). R = 3r gives a deltoid; R = 4r an astroid; R = n·r an n-cusped star.
inline vec2 hypocycloidPoint(float R, float r, float t) {
    return hypotrochoidPoint(R, r, r, t);
}

// --- Polyline samplers over the roll-angle range [tStart, tEnd] (samples + 1 points, samples >= 1). ---

inline std::vector<vec2> trochoidPolyline(float r, float d, float tStart, float tEnd, int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = tStart + (tEnd - tStart) * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(trochoidPoint(r, d, t));
    }
    return out;
}

inline std::vector<vec2> epitrochoidPolyline(float R, float r, float d, float tStart, float tEnd,
                                             int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = tStart + (tEnd - tStart) * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(epitrochoidPoint(R, r, d, t));
    }
    return out;
}

inline std::vector<vec2> hypotrochoidPolyline(float R, float r, float d, float tStart, float tEnd,
                                              int samples) {
    if (samples < 1) {
        samples = 1;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples + 1));
    for (int i = 0; i <= samples; ++i) {
        const float t = tStart + (tEnd - tStart) * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(hypotrochoidPoint(R, r, d, t));
    }
    return out;
}

} // namespace maz::math
