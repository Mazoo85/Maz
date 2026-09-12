#include "maz/fx/Particles.hpp"

#include <algorithm>
#include <cmath>

namespace maz::fx {

namespace {
float lerp(float a, float b, float t) { return a + (b - a) * t; }
render::Color lerp(const render::Color& a, const render::Color& b, float t) {
    return render::Color{lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t),
                         lerp(a.a, b.a, t)};
}
} // namespace

ParticleSystem::ParticleSystem(uint32_t capacity) {
    m_pool.resize(capacity ? capacity : 1);
}

float ParticleSystem::frand(float lo, float hi) {
    // xorshift64* — cheap, deterministic, no global state.
    m_rng ^= m_rng >> 12;
    m_rng ^= m_rng << 25;
    m_rng ^= m_rng >> 27;
    const uint64_t v = m_rng * 0x2545f4914f6cdd1dull;
    const float unit = static_cast<float>((v >> 40) & 0xffffff) / static_cast<float>(0x1000000);
    return lo + (hi - lo) * unit;
}

void ParticleSystem::emit(const BurstDesc& b) {
    for (int i = 0; i < b.count; ++i) {
        Particle& p = m_pool[m_cursor];
        m_cursor = (m_cursor + 1) % static_cast<uint32_t>(m_pool.size());

        const float angle = frand(b.angleMin, b.angleMax);
        const float speed = frand(b.speedMin, b.speedMax);
        p.x = b.x;
        p.y = b.y;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.maxLife = frand(b.lifeMin, b.lifeMax);
        p.life = p.maxLife;
        p.sizeStart = b.sizeStart;
        p.sizeEnd = b.sizeEnd;
        p.gravity = b.gravity;
        p.drag = b.drag;
        p.c0 = b.colorStart;
        p.c1 = b.colorEnd;
        p.active = true;
    }
}

void ParticleSystem::update(float dt) {
    uint32_t alive = 0;
    for (Particle& p : m_pool) {
        if (!p.active) {
            continue;
        }
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        p.vy += p.gravity * dt;
        // Optional attractor: radial pull toward (m_attX, m_attY) plus a perpendicular swirl.
        if (m_attStrength != 0.0f || m_attSwirl != 0.0f) {
            const float dx = m_attX - p.x;
            const float dy = m_attY - p.y;
            const float dist = std::sqrt(dx * dx + dy * dy) + 1e-3f;
            const float nx = dx / dist;
            const float ny = dy / dist;
            p.vx += (nx * m_attStrength - ny * m_attSwirl) * dt;
            p.vy += (ny * m_attStrength + nx * m_attSwirl) * dt;
        }
        const float damp = std::max(0.0f, 1.0f - p.drag * dt);
        p.vx *= damp;
        p.vy *= damp;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        ++alive;
    }
    m_alive = alive;
}

void ParticleSystem::draw(render::Renderer& renderer, render::TextureHandle texture) const {
    for (const Particle& p : m_pool) {
        if (!p.active) {
            continue;
        }
        const float t = 1.0f - p.life / p.maxLife; // 0 at birth -> 1 at death
        const float size = lerp(p.sizeStart, p.sizeEnd, t);
        render::SpriteDesc s;
        s.x = p.x - size * 0.5f;
        s.y = p.y - size * 0.5f;
        s.width = size;
        s.height = size;
        s.color = lerp(p.c0, p.c1, t);
        renderer.drawSprite(texture, s);
    }
}

void ParticleSystem::clear() {
    for (Particle& p : m_pool) {
        p.active = false;
    }
    m_alive = 0;
}

} // namespace maz::fx
