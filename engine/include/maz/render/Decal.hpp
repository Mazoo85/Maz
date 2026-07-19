#pragma once

#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <optional>

// maz::render decal projection — the math behind Godot's Decal node: an oriented box that stamps a texture
// onto whatever surface lies inside it (bullet holes, blood, posters, tire tracks). This is the CPU core:
// given a decal box and a world-space surface point + normal, `projectDecal` returns the texture UV to
// sample and a blend alpha, or nothing when the point falls outside the box or the surface faces away from
// the projector. The box is defined by a center, an orthonormal frame (right / up / forward), and half
// extents; it projects along its local -up, so the footprint is the right×forward plane and `up` is the
// projection depth. A normal-fade cutoff rejects surfaces that don't face the projector, matching Godot's
// `normal_fade`. Pure math — unit-tested headlessly; the actual texture blend happens on the GPU.
//
// Scope note (honest): axis-aligned-in-local-space projection with edge-inclusive bounds and a linear
// normal fade. It does not do the GPU decal blend, per-decal albedo/emission mixing, or upper/lower depth
// fade curves; those are the renderer's job. Axes are assumed orthonormal and unit length.
namespace maz::render {

struct Decal {
    math::vec3 center{0.0f, 0.0f, 0.0f};
    math::vec3 right{1.0f, 0.0f, 0.0f};      // local +X (footprint width)
    math::vec3 up{0.0f, 1.0f, 0.0f};         // local +Y (projection depth; casts along -up)
    math::vec3 forward{0.0f, 0.0f, 1.0f};    // local +Z (footprint height)
    math::vec3 halfExtents{1.0f, 1.0f, 1.0f};
    float normalCutoff = 0.0f; // require dot(surfaceNormal, up) >= this to receive the decal
};

struct DecalSample {
    float u, v;   // [0,1] footprint coordinates
    float alpha;  // blend weight from the normal fade, in [0,1]
};

// Project `point` (with surface `normal`) into `decal`. Returns the UV + alpha, or nullopt if the point is
// outside the box or the surface faces away from the projector.
inline std::optional<DecalSample> projectDecal(const Decal& decal, const math::vec3& point,
                                               const math::vec3& normal) {
    const math::vec3 d = point - decal.center;
    const float lx = glm::dot(d, decal.right);
    const float ly = glm::dot(d, decal.up);
    const float lz = glm::dot(d, decal.forward);

    if (std::fabs(lx) > decal.halfExtents.x || std::fabs(ly) > decal.halfExtents.y ||
        std::fabs(lz) > decal.halfExtents.z) {
        return std::nullopt;
    }

    const float nlen = std::sqrt(glm::dot(normal, normal));
    const math::vec3 n = nlen > 1e-8f ? normal / nlen : decal.up;
    const float alignment = glm::dot(n, decal.up);
    if (alignment < decal.normalCutoff) return std::nullopt;

    DecalSample s;
    s.u = decal.halfExtents.x > 0.0f ? lx / (2.0f * decal.halfExtents.x) + 0.5f : 0.5f;
    s.v = decal.halfExtents.z > 0.0f ? lz / (2.0f * decal.halfExtents.z) + 0.5f : 0.5f;
    s.alpha = alignment < 0.0f ? 0.0f : (alignment > 1.0f ? 1.0f : alignment);
    return s;
}

} // namespace maz::render
