#pragma once

#include "maz/math/Geometry3D.hpp"

#include <cmath>
#include <vector>

// maz::render CPU lightmap baker — the offline "burn the lighting into a texture" step behind Godot's
// LightmapGI. A lightmap stores precomputed lighting per surface point so static geometry looks lit
// without paying for lights at runtime. This bakes the *direct* term: for each surfel (a world-space
// point + normal — in a full pipeline these are the unwrapped texel centers), it sums every light's
// contribution (N·L, with distance falloff for point lights) and traces a shadow ray against the
// occluder triangles so geometry casts hard shadows into the map. It reuses the header-only
// `math::segmentIntersectsTriangle` ray test, so it is pure CPU and fully unit-testable; only *sampling*
// the resulting map at draw time needs a GPU.
//
// Scope note (honest): direct lighting + hard shadows only. It does not do indirect/bounce GI, area-light
// softness, or the UV-atlas unwrap (the caller supplies the surfels). Those are documented follow-ups.
namespace maz::render {

struct Surfel {
    math::vec3 position;
    math::vec3 normal; // expected unit length
};

enum class BakeLightKind { Directional, Point };

struct BakeLight {
    BakeLightKind kind = BakeLightKind::Directional;
    math::vec3 direction{0.0f, -1.0f, 0.0f}; // directional: the direction light travels
    math::vec3 position{0.0f, 0.0f, 0.0f};   // point: world position
    math::vec3 color{1.0f, 1.0f, 1.0f};      // linear RGB intensity
    float range = 0.0f;                      // point: 0 = no falloff; else linear cutoff at `range`
};

struct BakeTriangle {
    math::vec3 a, b, c;
};

struct LightmapBakeOptions {
    math::vec3 ambient{0.0f, 0.0f, 0.0f};
    float shadowBias = 1e-3f; // push the shadow-ray origin off the surface to avoid self-shadowing
    bool castShadows = true;
};

namespace detail {

inline bool segmentBlocked(const math::vec3& from, const math::vec3& to,
                           const std::vector<BakeTriangle>& occluders) {
    for (const BakeTriangle& t : occluders) {
        if (math::segmentIntersectsTriangle(from, to, t.a, t.b, t.c).has_value()) return true;
    }
    return false;
}

} // namespace detail

// Bake per-surfel direct irradiance. Output[i] is the linear RGB lighting at surfels[i].
inline std::vector<math::vec3> bakeLightmap(const std::vector<Surfel>& surfels,
                                            const std::vector<BakeLight>& lights,
                                            const std::vector<BakeTriangle>& occluders,
                                            const LightmapBakeOptions& opts = {}) {
    std::vector<math::vec3> out;
    out.reserve(surfels.size());

    for (const Surfel& s : surfels) {
        math::vec3 color = opts.ambient;
        const float nlen = std::sqrt(glm::dot(s.normal, s.normal));
        const math::vec3 n = nlen > 1e-8f ? s.normal / nlen : math::vec3{0.0f, 1.0f, 0.0f};
        const math::vec3 origin = s.position + n * opts.shadowBias;

        for (const BakeLight& light : lights) {
            math::vec3 L{0.0f, 0.0f, 0.0f};
            float atten = 1.0f;
            math::vec3 shadowTarget{0.0f, 0.0f, 0.0f};

            if (light.kind == BakeLightKind::Directional) {
                const float dlen = std::sqrt(glm::dot(light.direction, light.direction));
                if (dlen < 1e-8f) continue;
                L = -light.direction / dlen; // direction from surfel toward the light
                shadowTarget = origin + L * 1.0e4f;
            } else {
                const math::vec3 toLight = light.position - s.position;
                const float dist = std::sqrt(glm::dot(toLight, toLight));
                if (dist < 1e-6f) continue;
                L = toLight / dist;
                shadowTarget = light.position;
                if (light.range > 0.0f) {
                    if (dist >= light.range) continue;
                    atten = 1.0f - dist / light.range; // linear falloff to 0 at `range`
                }
            }

            const float ndotl = glm::dot(n, L);
            if (ndotl <= 0.0f) continue;

            if (opts.castShadows && !occluders.empty() &&
                detail::segmentBlocked(origin, shadowTarget, occluders)) {
                continue;
            }

            color += light.color * (ndotl * atten);
        }

        if (color.x < 0.0f) color.x = 0.0f;
        if (color.y < 0.0f) color.y = 0.0f;
        if (color.z < 0.0f) color.z = 0.0f;
        out.push_back(color);
    }
    return out;
}

} // namespace maz::render
