#pragma once

#include "maz/render/Lightmap.hpp" // Surfel + math/Geometry3D (rayIntersectsTriangle, glm)

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render one-bounce global-illumination gather — the INDIRECT half of Godot's LightmapGI/SDFGI, the
// piece the M96 direct-only `bakeLightmap` explicitly left as a follow-up. Direct lighting only tells a
// surface how much it sees the lights; GI is what makes a red wall bleed pink onto the floor beside it and a
// shadowed nook still read softly lit from the open sky. This bakes that: for each receiver surfel it fires
// a cosine-weighted hemisphere of rays; each ray either strikes a scene PATCH (a triangle carrying an
// emitted+reflected radiance — e.g. the floor's direct-lit color, or a glowing surface) and collects that
// radiance, or escapes to the SKY and collects the sky color. The average of those samples is the incoming
// radiance (irradiance / pi) arriving at the surfel — multiply by albedo for the reflected color, or add it
// to the direct lightmap term for full GI. Cosine weighting makes the estimator exact for a constant field
// (an open surfel under a uniform sky returns exactly the sky color), and the samples come from a
// deterministic Hammersley sequence, so the bake is reproducible and unit-testable without a GPU.
//
// Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the gather + visibility math is CPU-verified here and is a
// real (if single-bounce) path-traced irradiance estimate. Iterating it to convergence over a full unwrapped
// UV atlas, multi-bounce, and streaming the result into an SDFGI probe volume are the heavier offline/GPU
// stages this provides the kernel for.
namespace maz::render {

// A scene triangle carrying the radiance it emits toward the gather (its own emission plus whatever direct
// light it already reflects). Feed the direct-lit surface colors here to get one bounce of indirect light.
struct GiPatch {
    math::vec3 a{0.0f, 0.0f, 0.0f};
    math::vec3 b{0.0f, 0.0f, 0.0f};
    math::vec3 c{0.0f, 0.0f, 0.0f};
    math::vec3 radiance{0.0f, 0.0f, 0.0f};
};

struct GiBakeOptions {
    math::vec3 skyColor{0.0f, 0.0f, 0.0f}; // radiance collected by rays that escape the scene
    int samples = 64;                      // hemisphere rays per surfel
    float maxDistance = 1.0e4f;            // gather radius
    float bias = 1e-3f;                    // push the ray origin off the surface to avoid self-hits
};

namespace detail {

// Van der Corput radical inverse in base 2 (the second Hammersley coordinate).
inline float radicalInverseVdC(std::uint32_t bits) {
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return static_cast<float>(bits) * 2.3283064365386963e-10f; // * 2^-32
}

// Nearest patch hit by ray (from + t*dir, t in (0, maxDist]). Returns its radiance via `hitRadiance`; the
// return value is true on hit.
inline bool nearestPatch(const math::vec3& from, const math::vec3& dir, float maxDist,
                         const std::vector<GiPatch>& patches, math::vec3& hitRadiance) {
    float best = maxDist;
    bool found = false;
    for (const GiPatch& p : patches) {
        const std::optional<math::vec3> hit = math::rayIntersectsTriangle(from, dir, p.a, p.b, p.c);
        if (!hit.has_value()) continue;
        const math::vec3 d = *hit - from;
        const float dist = std::sqrt(glm::dot(d, d));
        if (dist > 1e-6f && dist < best) {
            best = dist;
            hitRadiance = p.radiance;
            found = true;
        }
    }
    return found;
}

} // namespace detail

// Gather one bounce of incoming radiance at a surfel. Returns the average sampled radiance, which equals the
// irradiance divided by pi (multiply by albedo for reflected color). An open surfel under a uniform sky
// returns exactly `skyColor`; occlusion by dark patches drives it toward those patches' radiance.
inline math::vec3 gatherIrradiance(const Surfel& s, const std::vector<GiPatch>& patches,
                                   const GiBakeOptions& o = {}) {
    const int n = o.samples > 0 ? o.samples : 1;
    const float nlen = std::sqrt(glm::dot(s.normal, s.normal));
    const math::vec3 nrm = nlen > 1e-8f ? s.normal / nlen : math::vec3{0.0f, 1.0f, 0.0f};

    // Orthonormal basis around the normal.
    const math::vec3 up = std::fabs(nrm.x) > 0.9f ? math::vec3{0.0f, 1.0f, 0.0f} : math::vec3{1.0f, 0.0f, 0.0f};
    const math::vec3 tan = glm::normalize(glm::cross(up, nrm));
    const math::vec3 bit = glm::cross(nrm, tan);
    const math::vec3 origin = s.position + nrm * o.bias;

    math::vec3 sum{0.0f, 0.0f, 0.0f};
    for (int i = 0; i < n; ++i) {
        // Hammersley (u1, u2) -> cosine-weighted hemisphere direction (pdf = cosθ/pi).
        const float u1 = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const float u2 = detail::radicalInverseVdC(static_cast<std::uint32_t>(i));
        const float r = std::sqrt(u1);
        const float phi = 6.28318530717958647692f * u2;
        const float lx = r * std::cos(phi);
        const float ly = r * std::sin(phi);
        const float lz = std::sqrt(std::max(0.0f, 1.0f - u1));
        const math::vec3 dir = glm::normalize(tan * lx + bit * ly + nrm * lz);

        math::vec3 rad{0.0f, 0.0f, 0.0f};
        if (detail::nearestPatch(origin, dir, o.maxDistance, patches, rad)) {
            sum += rad;
        } else {
            sum += o.skyColor;
        }
    }
    return sum / static_cast<float>(n);
}

// Bake indirect/sky irradiance for a set of surfels (one hemisphere gather each). Output[i] is the average
// incoming radiance at surfels[i]; multiply by albedo for reflected color, or add to the direct lightmap.
inline std::vector<math::vec3> bakeIndirect(const std::vector<Surfel>& surfels,
                                            const std::vector<GiPatch>& patches,
                                            const GiBakeOptions& o = {}) {
    std::vector<math::vec3> out;
    out.reserve(surfels.size());
    for (const Surfel& s : surfels) out.push_back(gatherIrradiance(s, patches, o));
    return out;
}

} // namespace maz::render
