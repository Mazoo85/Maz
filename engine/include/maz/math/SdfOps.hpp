#pragma once

#include <algorithm>
#include <cmath>

// maz::math signed-distance combination operators — the composition layer that turns individual distance
// fields (Sdf2D.hpp, or any distance value) into compound shapes. Hard boolean ops (union/intersect/subtract)
// come from min/max; the SMOOTH variants blend two shapes with a rounded seam of width `k` — exactly what
// gives metaballs, soft merges, blobby creatures, welded UI shapes and organic terrain their look. Plus the
// per-shape modifiers: `round` (fillet every edge by r), `annular` (turn a solid into a hollow shell/outline
// of thickness 2r), and `interpolate` (morph between two shapes). These are plain float combinators, so they
// work on 2D or 3D distances alike; game::Csg wraps the 3D field case as std::function, this is the light
// value-level primitive Godot has no equivalent for. Header-only, std-only, deterministic.
namespace maz::math {

namespace detail {
inline float sdfClamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
} // namespace detail

// --- Hard boolean operators (exact SDFs where the inputs are). ---
inline float opUnion(float a, float b) { return std::min(a, b); }
inline float opIntersect(float a, float b) { return std::max(a, b); }
inline float opSubtract(float a, float b) { return std::max(a, -b); } // a minus b

// --- Per-shape modifiers. ---
// Fillet: round every edge of the shape by radius `r` (shrinks the surface outward by r).
inline float opRound(float d, float r) { return d - r; }
// Onion/annular: hollow the shape into a shell of half-thickness `r` around its original boundary.
inline float opAnnular(float d, float r) { return std::fabs(d) - r; }
// Morph between fields `a` and `b` by `t` in [0,1] (linear blend of the distances).
inline float opInterpolate(float a, float b, float t) { return a * (1.0f - t) + b * t; }

// --- Smooth boolean operators (polynomial blend of width k; k<=0 reduces to the hard op). ---
inline float opSmoothUnion(float a, float b, float k) {
    if (k <= 0.0f) {
        return std::min(a, b);
    }
    const float h = detail::sdfClamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return b * (1.0f - h) + a * h - k * h * (1.0f - h);
}
inline float opSmoothIntersect(float a, float b, float k) {
    if (k <= 0.0f) {
        return std::max(a, b);
    }
    const float h = detail::sdfClamp(0.5f - 0.5f * (b - a) / k, 0.0f, 1.0f);
    return b * (1.0f - h) + a * h + k * h * (1.0f - h);
}
inline float opSmoothSubtract(float a, float b, float k) { // a minus b, with a smooth seam
    if (k <= 0.0f) {
        return std::max(a, -b);
    }
    const float h = detail::sdfClamp(0.5f - 0.5f * (a + b) / k, 0.0f, 1.0f);
    return a * (1.0f - h) + (-b) * h + k * h * (1.0f - h);
}

} // namespace maz::math
