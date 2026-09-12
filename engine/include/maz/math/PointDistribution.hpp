#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::math even point distributions — spread N points as uniformly as possible over a sphere, a hemisphere,
// or a disc, with no random number generator (fully deterministic). The trick is the golden angle
// (pi*(3-sqrt5) ≈ 137.5°): stepping each successive point by that angle never lets points line up into spokes
// or rings, so coverage stays even at any count. This is the workhorse behind uniform DIRECTION sampling —
// ambient-occlusion / global-illumination rays, reflection-probe placement, spawn directions, LOD impostor
// captures — and even POINT scatter — star fields, point clouds, dotted patterns, blue-noise-ish sample
// kernels. The engine already used this spiral inline in a couple of shaders (SoftShadow2D, MeshAO); this
// exposes it as a reusable, unit-tested primitive. Pairs naturally with SphericalCoords (M635). Header-only,
// pure, deterministic — same N always yields the same points.
namespace maz::math {

// The golden angle in radians: pi * (3 - sqrt(5)). Consecutive multiples never repeat a direction.
inline constexpr float kGoldenAngle = 2.39996322972865332f;

// N unit vectors spread evenly over the whole unit sphere (the "Fibonacci sphere"). z marches linearly from
// near +1 to near -1 while the azimuth advances by the golden angle, giving near-uniform surface coverage.
inline std::vector<vec3> fibonacciSphere(int n) {
    std::vector<vec3> out;
    if (n <= 0) return out;
    out.reserve(static_cast<std::size_t>(n));
    const float fn = static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        const float fi = static_cast<float>(i);
        const float z = 1.0f - (2.0f * fi + 1.0f) / fn; // in (-1, 1), symmetric about 0
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        const float a = fi * kGoldenAngle;
        out.push_back(vec3(r * std::cos(a), r * std::sin(a), z));
    }
    return out;
}

// N unit vectors spread evenly over the UPPER hemisphere (z >= 0) — the usual need for AO/GI/irradiance
// sampling around a surface normal (rotate these into the normal's frame). Uniform over the hemisphere area.
inline std::vector<vec3> fibonacciHemisphere(int n) {
    std::vector<vec3> out;
    if (n <= 0) return out;
    out.reserve(static_cast<std::size_t>(n));
    const float fn = static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        const float fi = static_cast<float>(i);
        const float z = 1.0f - (fi + 0.5f) / fn; // in (0, 1]
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        const float a = fi * kGoldenAngle;
        out.push_back(vec3(r * std::cos(a), r * std::sin(a), z));
    }
    return out;
}

// N points spread evenly across a disc of the given radius (the Vogel / sunflower spiral). Radius grows as
// sqrt(index) so the areal density is uniform; the golden angle keeps the angular spacing even. Great for
// soft-shadow / depth-of-field sample kernels and dotted radial layouts.
inline std::vector<vec2> vogelDisk(int n, float radius = 1.0f) {
    std::vector<vec2> out;
    if (n <= 0) return out;
    out.reserve(static_cast<std::size_t>(n));
    const float fn = static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        const float fi = static_cast<float>(i);
        const float r = radius * std::sqrt((fi + 0.5f) / fn); // sqrt keeps areal density even
        const float a = fi * kGoldenAngle;
        out.push_back(vec2(r * std::cos(a), r * std::sin(a)));
    }
    return out;
}

} // namespace maz::math
