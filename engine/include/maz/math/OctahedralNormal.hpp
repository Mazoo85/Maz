#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <cmath>

// maz::math octahedral unit-vector encoding — pack a unit vector (a normal, a direction) into just TWO
// numbers in [-1,1], and unpack it back, with barely any error. This is the standard way modern renderers
// store normals compactly: in a G-buffer (deferred shading), a compressed normal map, a light-probe
// direction, or any place three floats per normal is too much bandwidth/memory. Octahedral mapping unfolds
// the sphere onto a square and is far more uniform (less error) than the old "store xy, reconstruct z"
// hemisphere trick. Two variants: full-sphere `octEncode`/`octDecode` (any direction) and `octEncodeHemi`/
// `octDecodeHemi` (a +Z hemisphere, e.g. view-space normals — uses the whole square for extra precision).
// Godot does this inside shaders only; this is the reusable CPU-side pair for baking and tools. Header-only,
// std-only, deterministic.
namespace maz::math {

namespace oct_detail {
inline float signNotZero(float v) { return v >= 0.0f ? 1.0f : -1.0f; }
inline vec3 norm3(const vec3& v) {
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return l > 1e-20f ? v * (1.0f / l) : v;
}
} // namespace oct_detail

// Encode a unit vector into a point in [-1,1]^2 (full sphere).
inline vec2 octEncode(const vec3& nIn) {
    using oct_detail::signNotZero;
    const float invL1 = 1.0f / (std::fabs(nIn.x) + std::fabs(nIn.y) + std::fabs(nIn.z));
    float x = nIn.x * invL1;
    float y = nIn.y * invL1;
    if (nIn.z < 0.0f) {
        const float ox = (1.0f - std::fabs(y)) * signNotZero(x);
        const float oy = (1.0f - std::fabs(x)) * signNotZero(y);
        x = ox;
        y = oy;
    }
    return vec2(x, y);
}

// Decode a point in [-1,1]^2 back to a unit vector (full sphere). Inverse of octEncode.
inline vec3 octDecode(const vec2& f) {
    using oct_detail::signNotZero;
    vec3 n(f.x, f.y, 1.0f - std::fabs(f.x) - std::fabs(f.y));
    if (n.z < 0.0f) {
        const float ox = (1.0f - std::fabs(n.y)) * signNotZero(n.x);
        const float oy = (1.0f - std::fabs(n.x)) * signNotZero(n.y);
        n.x = ox;
        n.y = oy;
    }
    return oct_detail::norm3(n);
}

// Encode a unit vector from the +Z hemisphere (n.z >= 0) into [-1,1]^2, using the full square for precision.
inline vec2 octEncodeHemi(const vec3& nIn) {
    const float invL1 = 1.0f / (std::fabs(nIn.x) + std::fabs(nIn.y) + nIn.z);
    const float fx = nIn.x * invL1;
    const float fy = nIn.y * invL1;
    return vec2(fx + fy, fx - fy); // rotate 45 degrees into the square
}

// Decode a +Z-hemisphere octahedral point back to a unit vector. Inverse of octEncodeHemi.
inline vec3 octDecodeHemi(const vec2& e) {
    const float fx = (e.x + e.y) * 0.5f;
    const float fy = (e.x - e.y) * 0.5f;
    vec3 n(fx, fy, 1.0f - std::fabs(fx) - std::fabs(fy));
    return oct_detail::norm3(n);
}

} // namespace maz::math
