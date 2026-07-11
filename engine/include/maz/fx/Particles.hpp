#pragma once

#include "maz/render/Renderer.hpp"

#include <cstdint>
#include <vector>

namespace maz::fx {

// Parameters for one burst of particles emitted from a point.
struct BurstDesc {
    int count = 16;
    float x = 0.0f, y = 0.0f;
    float speedMin = 40.0f, speedMax = 160.0f;
    float angleMin = 0.0f, angleMax = 6.2831853f; // radians; default = full circle
    float lifeMin = 0.3f, lifeMax = 0.7f;
    float sizeStart = 10.0f, sizeEnd = 2.0f;
    render::Color colorStart{1.0f, 1.0f, 1.0f, 1.0f};
    render::Color colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
    float gravity = 0.0f; // added to vy each second (+y = down)
    float drag = 0.0f;    // fraction of velocity shed per second
};

// A fixed-capacity 2D particle pool. Renderer-agnostic: draw() emits one tinted, size/alpha-faded
// sprite per live particle, so it works with any Renderer and respects the active camera.
class ParticleSystem {
public:
    explicit ParticleSystem(uint32_t capacity = 2048);

    void emit(const BurstDesc& burst);
    void update(float dt);
    void draw(render::Renderer& renderer, render::TextureHandle texture) const;
    void clear();

    uint32_t alive() const { return m_alive; }

private:
    struct Particle {
        float x, y, vx, vy;
        float life, maxLife;
        float sizeStart, sizeEnd;
        float gravity, drag;
        render::Color c0, c1;
        bool active;
    };

    float frand(float lo, float hi);

    std::vector<Particle> m_pool;
    uint32_t m_alive = 0;
    uint32_t m_cursor = 0; // round-robin allocation (overwrites oldest when full)
    uint64_t m_rng = 0x9e3779b97f4a7c15ull;
};

} // namespace maz::fx
