#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <vector>

namespace maz::game {

// Normal-mapped 2D lighting — Godot's Light2D with a normal map. A flat 2D sprite carries a NORMAL MAP
// (a per-texel surface normal, +z pointing out of the screen); a 2D light then shades each texel by how
// squarely its normal faces the light, so a painted-flat brick wall or ground catches light directionally
// and reads as three-dimensional — bumps lit on the side facing the light, shadowed on the far side, the
// highlight sliding across as the light moves. Earlier lights (M89/M101) were flat-coloured pools with
// occluder shadows but no per-texel normal response; this adds it. It is pure vector math (a 3D
// point-light Lambert term + smooth distance attenuation), deterministic and headless-testable; a
// renderer just fills each shaded texel/cell with the returned colour.

// A 2D point light floating at `height` above the surface plane. `height` sets the grazing angle: a low
// light rakes across the bumps (long shadows / strong side-light), a high light lights them flatter.
struct PointLight2D {
    math::vec2 pos{0.0f, 0.0f}; // position on the surface plane (pixels/world units)
    float height = 40.0f;       // z above the plane; larger => more top-down
    math::vec3 color{1.0f, 1.0f, 1.0f};
    float energy = 1.0f;        // intensity multiplier
    float range = 300.0f;       // attenuation radius; past this the light contributes ~nothing
};

// Additive contribution of one light to a surface texel at world `p` whose unit surface normal is `n`
// (tangent space, +z out of the screen) and albedo `albedo` (linear rgb). Lambert N·L in 3D with the
// light direction taken from the texel up to the light at its height, times a smooth (1−(d/range)²)²
// falloff. Returns zero when the texel faces away from the light or lies outside its range.
inline math::vec3 shadePointLight(math::vec2 p, math::vec3 n, math::vec3 albedo, const PointLight2D& L) {
    const math::vec2 d2 = L.pos - p;
    const float dist2 = d2.x * d2.x + d2.y * d2.y;
    const float range2 = L.range * L.range;
    if (dist2 >= range2) {
        return math::vec3(0.0f);
    }
    // 3D direction from the texel (on the z=0 plane) up to the light (at z=height).
    math::vec3 toLight(d2.x, d2.y, L.height);
    const float len = std::sqrt(glm::dot(toLight, toLight));
    if (len < 1e-6f) {
        return albedo * L.color * L.energy; // directly under the light
    }
    toLight /= len;
    const float ndotl = glm::dot(n, toLight);
    if (ndotl <= 0.0f) {
        return math::vec3(0.0f); // facing away
    }
    // Smooth radial falloff on the in-plane distance: 1 at the light, 0 at the range edge.
    const float t = 1.0f - dist2 / range2; // in (0,1]
    const float atten = t * t;
    return albedo * L.color * (L.energy * ndotl * atten);
}

// Shade a texel under several lights plus a flat ambient term, clamped to [0,1] per channel. `ambient`
// is added as albedo·ambient so unlit areas keep the base colour instead of going black.
inline math::vec3 shadeSurface(math::vec2 p, math::vec3 n, math::vec3 albedo,
                               const std::vector<PointLight2D>& lights, math::vec3 ambient) {
    math::vec3 c = albedo * ambient;
    for (const PointLight2D& L : lights) {
        c += shadePointLight(p, n, albedo, L);
    }
    c.x = std::min(1.0f, std::max(0.0f, c.x));
    c.y = std::min(1.0f, std::max(0.0f, c.y));
    c.z = std::min(1.0f, std::max(0.0f, c.z));
    return c;
}

// Convenience: unpack a normal-map RGB texel (each channel in [0,1], the usual "blue" encoding where
// (0.5,0.5,1) is flat) into a unit surface normal (+z out of screen).
inline math::vec3 decodeNormal(math::vec3 rgb) {
    math::vec3 n(rgb.x * 2.0f - 1.0f, rgb.y * 2.0f - 1.0f, rgb.z * 2.0f - 1.0f);
    const float len = std::sqrt(glm::dot(n, n));
    return len > 1e-6f ? n / len : math::vec3(0.0f, 0.0f, 1.0f);
}

} // namespace maz::game
