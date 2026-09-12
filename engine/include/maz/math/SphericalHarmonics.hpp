#pragma once

#include "maz/math/VectorOps.hpp" // vec3

#include <array>
#include <cstddef>

// maz::math order-2 spherical harmonics (9 coefficients) for ambient / diffuse irradiance — the compact
// "light probe" representation Godot bakes into LightmapGI and uses for ambient lighting. Instead of storing
// a full environment cubemap, an entire low-frequency lighting environment is captured in 9 RGB numbers per
// probe: cheap to store, cheap to evaluate, and smooth to interpolate between probes. You project directional
// radiance samples (from a captured environment, a sky model, or point lights) into an `ShL2` with
// `addSample`, then reconstruct the diffuse irradiance arriving on any surface normal with `irradiance`,
// which applies the standard clamped-cosine (Lambert) convolution — the same band scaling
// (A0 = pi, A1 = 2pi/3, A2 = pi/4) Godot and the Ramamoorthi/Hanrahan formulation use. Pure math, no GPU:
// projecting and evaluating are fully unit-testable here (a lit frame from the result is the GPU's job).
//
// Scope note (honest): order-2 (L2, 9 coefficients) real SH with the cosine-lobe irradiance convolution.
// Rotation of an SH set and windowing/deringing are documented follow-ups.
namespace maz::math {

// The 9 real spherical-harmonic basis functions evaluated at a unit direction, in the conventional order
// (l,m): (0,0) | (1,-1)(1,0)(1,1) | (2,-2)(2,-1)(2,0)(2,1)(2,2). Constants are the standard normalized real
// SH coefficients.
inline std::array<float, 9> shBasis(const vec3& d) {
    const float x = d.x, y = d.y, z = d.z;
    return {
        0.2820947918f,               // Y0,0
        0.4886025119f * y,           // Y1,-1
        0.4886025119f * z,           // Y1,0
        0.4886025119f * x,           // Y1,1
        1.0925484306f * x * y,       // Y2,-2
        1.0925484306f * y * z,       // Y2,-1
        0.3153915652f * (3.0f * z * z - 1.0f), // Y2,0
        1.0925484306f * x * z,       // Y2,1
        0.5462742153f * (x * x - y * y),       // Y2,2
    };
}

// Order-2 SH light probe: 9 RGB coefficients.
struct ShL2 {
    vec3 c[9] = {vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f),
                 vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f)};

    // Accumulate one radiance sample arriving FROM `dir` with the given colour, weighted by its solid angle
    // (use 4*pi/N for N uniformly distributed samples). Projects the sample onto the 9 basis functions.
    void addSample(const vec3& dir, const vec3& color, float weight) {
        const std::array<float, 9> b = shBasis(dir);
        for (std::size_t i = 0; i < 9; ++i) c[i] += color * (b[i] * weight);
    }

    void clear() {
        for (std::size_t i = 0; i < 9; ++i) c[i] = vec3(0.0f);
    }
};

// Reconstruct the diffuse irradiance E(n) arriving on a surface with unit normal `n`, applying the
// clamped-cosine convolution: E(n) = sum_l A_l * sum_m L_lm Y_lm(n), with A0 = pi, A1 = 2*pi/3, A2 = pi/4.
// For a Lambertian surface the outgoing radiance is albedo/pi * E(n). Uniform radiance C over the sphere
// yields E = pi*C for every normal (the analytic ground truth used to verify this).
inline vec3 shIrradiance(const ShL2& sh, const vec3& n) {
    constexpr float kPiF = 3.14159265358979323846f;
    constexpr float a0 = kPiF;              // band 0
    constexpr float a1 = 2.0f * kPiF / 3.0f; // band 1
    constexpr float a2 = kPiF / 4.0f;        // band 2
    const std::array<float, 9> b = shBasis(n);
    vec3 e = sh.c[0] * (a0 * b[0]);
    e += sh.c[1] * (a1 * b[1]);
    e += sh.c[2] * (a1 * b[2]);
    e += sh.c[3] * (a1 * b[3]);
    e += sh.c[4] * (a2 * b[4]);
    e += sh.c[5] * (a2 * b[5]);
    e += sh.c[6] * (a2 * b[6]);
    e += sh.c[7] * (a2 * b[7]);
    e += sh.c[8] * (a2 * b[8]);
    return e;
}

} // namespace maz::math
