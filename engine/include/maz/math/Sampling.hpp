#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <cmath>

// maz::math Monte-Carlo sampling warps — map a pair of uniform [0,1) random numbers onto a disk, triangle,
// sphere or hemisphere with the CORRECT distribution. These are the building blocks of every stochastic
// rendering / simulation task the engine does on the CPU: scattering rays for a GI/AO bake, sampling an area
// light or an environment, jittering a lens for depth-of-field / bokeh, emitting particles uniformly over a
// surface, or blue-noise-ish placement. Getting the warp right matters — a naive (r=u, theta=2*pi*v) disk
// clumps points at the centre; the concentric and sqrt maps here are area-uniform, and the cosine-hemisphere
// map concentrates samples toward the pole exactly as importance sampling a Lambertian surface needs. Godot
// exposes none of these. Deterministic, header-only, std-only. (+Z is "up" for the hemisphere maps.)
namespace maz::math {

// Uniform point in the unit disk via Shirley's concentric (area-preserving, low-distortion) map. `u1`,`u2`
// in [0,1). Corners of the square map to the disk rim; the centre stays the centre.
inline vec2 sampleConcentricDisk(float u1, float u2) {
    const float a = 2.0f * u1 - 1.0f;
    const float b = 2.0f * u2 - 1.0f;
    if (a == 0.0f && b == 0.0f) {
        return vec2(0.0f, 0.0f);
    }
    const float quarterPi = 0.78539816339744831f;
    float r, phi;
    if (a * a > b * b) {
        r = a;
        phi = quarterPi * (b / a);
    } else {
        r = b;
        phi = 2.0f * quarterPi - quarterPi * (a / b);
    }
    return vec2(r * std::cos(phi), r * std::sin(phi));
}

// Uniform point in the unit disk via the polar sqrt map (radius = sqrt(u1) makes the area density uniform).
inline vec2 sampleUniformDisk(float u1, float u2) {
    const float r = std::sqrt(u1);
    const float theta = 6.28318530717958648f * u2;
    return vec2(r * std::cos(theta), r * std::sin(theta));
}

// Uniform barycentric coordinates (b0, b1) inside a triangle; the third is 1 - b0 - b1. Both >= 0, sum <= 1.
inline vec2 sampleUniformTriangle(float u1, float u2) {
    const float su0 = std::sqrt(u1);
    return vec2(1.0f - su0, u2 * su0);
}

// Cosine-weighted direction on the +Z hemisphere (Malley's method: project a uniform-disk sample up). The
// probability density is proportional to cos(theta) — the correct importance sampling for a diffuse surface.
inline vec3 sampleCosineHemisphere(float u1, float u2) {
    const vec2 d = sampleConcentricDisk(u1, u2);
    const float z = std::sqrt(std::fmax(0.0f, 1.0f - d.x * d.x - d.y * d.y));
    return vec3(d.x, d.y, z);
}

// Uniform direction on the +Z hemisphere (constant density over solid angle).
inline vec3 sampleUniformHemisphere(float u1, float u2) {
    const float z = u1; // in [0,1)
    const float r = std::sqrt(std::fmax(0.0f, 1.0f - z * z));
    const float phi = 6.28318530717958648f * u2;
    return vec3(r * std::cos(phi), r * std::sin(phi), z);
}

// Uniform direction on the whole unit sphere (constant density over solid angle).
inline vec3 sampleUniformSphere(float u1, float u2) {
    const float z = 1.0f - 2.0f * u1; // in (-1,1]
    const float r = std::sqrt(std::fmax(0.0f, 1.0f - z * z));
    const float phi = 6.28318530717958648f * u2;
    return vec3(r * std::cos(phi), r * std::sin(phi), z);
}

// Uniform point INSIDE the unit ball (solid sphere) — for volumetric emission (spawn particles in a spherical
// volume), a random offset within a radius, or sampling a spherical light's volume. Warps a uniform surface
// direction by radius = cbrt(u3): the cube-root makes the density uniform per unit VOLUME (a naive radius = u3
// clumps points near the centre, since a thin shell at radius r has area ~ r^2). `u1`,`u2`,`u3` in [0,1).
inline vec3 sampleUniformBall(float u1, float u2, float u3) {
    return sampleUniformSphere(u1, u2) * std::cbrt(u3);
}

} // namespace maz::math
