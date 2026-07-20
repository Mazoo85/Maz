#pragma once

#include "maz/render/GlobalIllumination.hpp" // GiPatch, GiBakeOptions, gatherIrradiance, Surfel

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render multi-bounce global illumination (progressive radiosity) — turns the SINGLE-bounce
// `gatherIrradiance` (M511) into the FULL bounced-light solution Godot's LightmapGI bakes. One bounce makes a
// red wall tint the floor beside it; but that tinted floor should then tint the wall back, and the ceiling,
// and so on — light keeps bouncing, each bounce dimmer than the last, until it settles. This iterates exactly
// that: every surface patch is BOTH an emitter and a receiver, and each pass re-gathers the incoming light at
// each patch from the current radiance of all the others, then sets the patch's new radiance to its own
// emission plus its albedo times what it received. Because every bounce loses energy (albedo < 1), the total
// grows but converges to a finite steady state — the hallmark of a correct radiosity solve, and exactly what
// the unit test pins down. Pure CPU (built on the tested hemisphere gather), so it verifies headlessly; the
// result is a per-patch radiance a lightmap/probe bake stores for the GPU to sample.
//
// Scope note (honest): a Jacobi-iterated patch radiosity (each patch = one triangle), diffuse only, `bounces`
// passes. Adaptive subdivision, form-factor caching, and hemicube acceleration are the offline optimizations
// on top; this is the correct-but-unoptimized reference the GPU/offline path can accelerate.
namespace maz::render {

// A surface patch that both emits and reflects. `emission` is any light it gives off on its own (a lamp,
// glowing sign, or the direct-lit color you seed from `bakeLightmap`); `albedo` is the fraction of incoming
// light it reflects (per channel). `radiance` is filled by the solve.
struct RadiosityPatch {
    math::vec3 a{0.0f, 0.0f, 0.0f};
    math::vec3 b{0.0f, 0.0f, 0.0f};
    math::vec3 c{0.0f, 0.0f, 0.0f};
    math::vec3 emission{0.0f, 0.0f, 0.0f};
    math::vec3 albedo{0.5f, 0.5f, 0.5f};
    math::vec3 radiance{0.0f, 0.0f, 0.0f}; // solved output (emission + reflected)
};

namespace detail {
inline math::vec3 patchCentroid(const RadiosityPatch& p) {
    return (p.a + p.b + p.c) * (1.0f / 3.0f);
}
inline math::vec3 patchNormal(const RadiosityPatch& p) {
    const math::vec3 n = glm::cross(p.b - p.a, p.c - p.a);
    const float len = std::sqrt(glm::dot(n, n));
    return len > 1e-12f ? n / len : math::vec3{0.0f, 1.0f, 0.0f};
}
} // namespace detail

// Run `bounces` radiosity passes over `patches`, filling each patch's `radiance`. `opt` supplies the sky
// color (light from rays that escape the scene) and the hemisphere sample count.
inline void bakeRadiosity(std::vector<RadiosityPatch>& patches, const GiBakeOptions& opt = {},
                          int bounces = 4) {
    const std::size_t count = patches.size();
    // Seed: a patch starts glowing with just its own emission (no bounced light yet).
    for (RadiosityPatch& p : patches) p.radiance = p.emission;

    for (int pass = 0; pass < bounces; ++pass) {
        // Snapshot the current radiance as emitters for this pass (Jacobi update).
        std::vector<GiPatch> emitters(count);
        for (std::size_t i = 0; i < count; ++i) {
            emitters[i].a = patches[i].a;
            emitters[i].b = patches[i].b;
            emitters[i].c = patches[i].c;
            emitters[i].radiance = patches[i].radiance;
        }

        std::vector<math::vec3> next(count);
        for (std::size_t i = 0; i < count; ++i) {
            Surfel s;
            s.normal = detail::patchNormal(patches[i]);
            s.position = detail::patchCentroid(patches[i]);
            const math::vec3 gathered = gatherIrradiance(s, emitters, opt);
            next[i] = patches[i].emission + patches[i].albedo * gathered; // component-wise reflect
        }
        for (std::size_t i = 0; i < count; ++i) patches[i].radiance = next[i];
    }
}

} // namespace maz::render
