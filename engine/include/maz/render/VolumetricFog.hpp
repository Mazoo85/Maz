#pragma once

#include "maz/math/Geometry3D.hpp"

#include <cmath>

// maz::render analytic volumetric fog — the CPU evaluation behind Godot's height/volumetric fog. It answers
// "how much fog is between the camera and this point, and what color does it leave the pixel?" via the
// Beer–Lambert law: it integrates the fog density along a view segment (with optional exponential
// height falloff — thicker low, thinner high, like real mist) to an optical depth, converts that to a
// blend factor `1 - exp(-optical)`, and mixes the scene color toward the fog color. Godot's volumetric fog
// raymarches a froxel volume on the GPU; this is the closed-form analytic evaluation, exact and unit-
// testable on the CPU (a renderer can use it directly for simple exponential fog or as a reference).
//
// Scope note (honest): homogeneous or exponential-height density with a single fog color. It does not do
// GPU froxel scattering, per-light in-scatter, noise/wind animation, or fog volume shapes; those are the
// renderer's job.
namespace maz::render {

struct FogParams {
    math::vec3 color{0.5f, 0.6f, 0.7f};
    float density = 0.0f;       // base extinction per unit distance at the reference height
    float heightFalloff = 0.0f; // 0 = uniform; >0 = density decays with height above heightRef
    float heightRef = 0.0f;     // world height at which density == `density`
};

// Integrated fog density (optical depth) along the segment from `from` to `to`.
inline float fogOpticalDepth(const FogParams& fog, const math::vec3& from, const math::vec3& to) {
    if (fog.density <= 0.0f) return 0.0f;
    const math::vec3 seg = to - from;
    const float dist = std::sqrt(glm::dot(seg, seg));
    if (dist <= 0.0f) return 0.0f;

    if (fog.heightFalloff <= 0.0f) {
        return fog.density * dist; // homogeneous
    }

    // density(h) = density * exp(-k * (h - href)), integrated over s in [0, dist] with h = from.y + dir.y*s.
    const float k = fog.heightFalloff;
    const float dirY = seg.y / dist;
    const float d0 = fog.density * std::exp(-k * (from.y - fog.heightRef));
    if (std::fabs(dirY) < 1e-6f) {
        return d0 * dist; // ray stays at (nearly) constant height
    }
    const float e = k * dirY;
    return d0 * (1.0f - std::exp(-e * dist)) / e;
}

// Fog blend factor in [0,1]: 0 = no fog, 1 = fully fogged.
inline float fogFactor(const FogParams& fog, const math::vec3& from, const math::vec3& to) {
    const float optical = fogOpticalDepth(fog, from, to);
    return 1.0f - std::exp(-optical);
}

// Mix `sceneColor` toward the fog color by the fog factor for the given view segment.
inline math::vec3 applyFog(const math::vec3& sceneColor, const FogParams& fog, const math::vec3& from,
                           const math::vec3& to) {
    const float f = fogFactor(fog, from, to);
    return sceneColor * (1.0f - f) + fog.color * f;
}

} // namespace maz::render
