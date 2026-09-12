#pragma once

#include <cstdint>
#include <utility>

#include "maz/core/Noise.hpp" // reuse the seeded Perlin potential (M85)

// maz::core::CurlNoise — a divergence-free ("incompressible") 2D flow field derived from the engine's
// Perlin noise. The flow vector at a point is the PERPENDICULAR of the noise's gradient, so it always
// runs along the contour lines of the potential rather than up or down them. Because a perpendicular
// gradient has (mathematically) zero divergence, particles carried by this field swirl and fold
// without ever bunching together or thinning out — exactly the look of smoke, wind, magic, fluid, and
// flowing hair that plain random or radial forces can't give. Built on top of Noise (M85, reused not
// duplicated); distinct from FlowField (pathfinding toward a goal) and Worley (cellular texture).
// Deterministic from a seed. Godot has no curl-noise primitive. Header-only, std-only.
namespace maz::core {

class CurlNoise {
public:
    explicit CurlNoise(std::uint64_t seed = 0, float eps = 1.0e-3f)
        : m_noise(seed), m_eps(eps > 0.0f ? eps : 1.0e-3f) {}

    // Central-difference gradient (dP/dx, dP/dy) of the single-octave noise potential.
    std::pair<float, float> gradient2(float x, float y) const {
        const float dPdx =
            (m_noise.noise2(x + m_eps, y) - m_noise.noise2(x - m_eps, y)) / (2.0f * m_eps);
        const float dPdy =
            (m_noise.noise2(x, y + m_eps) - m_noise.noise2(x, y - m_eps)) / (2.0f * m_eps);
        return {dPdx, dPdy};
    }

    // Divergence-free flow vector = perpendicular of the gradient: (dP/dy, -dP/dx). Perpendicular to
    // the gradient by construction, so it follows the potential's contour lines.
    std::pair<float, float> curl2(float x, float y) const {
        const std::pair<float, float> g = gradient2(x, y);
        return {g.second, -g.first};
    }

    // Curl of a multi-octave fbm potential — the same swirl at several scales at once (turbulence).
    std::pair<float, float> curlFbm2(float x, float y, int octaves = 4) const {
        const float dPdx =
            (m_noise.fbm2(x + m_eps, y, octaves) - m_noise.fbm2(x - m_eps, y, octaves)) / (2.0f * m_eps);
        const float dPdy =
            (m_noise.fbm2(x, y + m_eps, octaves) - m_noise.fbm2(x, y - m_eps, octaves)) / (2.0f * m_eps);
        return {dPdy, -dPdx};
    }

    const Noise& noise() const { return m_noise; }

private:
    Noise m_noise;
    float m_eps;
};

} // namespace maz::core
