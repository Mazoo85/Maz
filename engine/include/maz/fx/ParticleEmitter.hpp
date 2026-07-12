#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace maz::fx {

// Particle emitter RESOURCE — Godot's CPUParticles2D. The existing fx::ParticleSystem is a runtime pool
// that emits point bursts with a linear start->end colour/size; a real emitter is a *resource* you author
// once and reuse, with (1) an EMISSION SHAPE (point / disk / ring / rectangle), (2) per-lifetime CURVES
// for scale and alpha (not just two endpoints), and (3) a multi-stop colour GRADIENT over lifetime. This
// header is that resource plus a DETERMINISTIC simulator: given a seed and a query time it returns every
// live particle's drawable state, so it unit-tests headlessly and renders a golden-stable snapshot. Pure
// math — no GPU, no global state — so it composes with any renderer.

// ---- a piecewise-linear 1-D curve over t in [0,1] (Godot Curve), for scale / alpha ramps ------------
class Curve {
public:
    void addPoint(float t, float v) {
        Pt p{t, v};
        auto it = m_pts.begin();
        while (it != m_pts.end() && it->t < t) {
            ++it;
        }
        m_pts.insert(it, p);
    }
    std::size_t size() const { return m_pts.size(); }
    // Clamp to the ends; linear between neighbours. Empty -> 0.
    float sample(float t) const {
        if (m_pts.empty()) {
            return 0.0f;
        }
        if (t <= m_pts.front().t) {
            return m_pts.front().v;
        }
        if (t >= m_pts.back().t) {
            return m_pts.back().v;
        }
        for (std::size_t i = 1; i < m_pts.size(); ++i) {
            if (t <= m_pts[i].t) {
                const float t0 = m_pts[i - 1].t, t1 = m_pts[i].t;
                const float u = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
                return glm::mix(m_pts[i - 1].v, m_pts[i].v, u);
            }
        }
        return m_pts.back().v;
    }

private:
    struct Pt {
        float t, v;
    };
    std::vector<Pt> m_pts;
};

// ---- a multi-stop colour ramp over t in [0,1] (Godot Gradient) --------------------------------------
inline render::Color lerpColor(const render::Color& a, const render::Color& b, float t) {
    return render::Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                         a.a + (b.a - a.a) * t};
}

class Gradient {
public:
    void addStop(float t, render::Color c) {
        Stop s{t, c};
        auto it = m_stops.begin();
        while (it != m_stops.end() && it->t < t) {
            ++it;
        }
        m_stops.insert(it, s);
    }
    std::size_t size() const { return m_stops.size(); }
    // Clamp to the ends; linear between stops. Empty -> white.
    render::Color sample(float t) const {
        if (m_stops.empty()) {
            return render::Color{1.0f, 1.0f, 1.0f, 1.0f};
        }
        if (t <= m_stops.front().t) {
            return m_stops.front().c;
        }
        if (t >= m_stops.back().t) {
            return m_stops.back().c;
        }
        for (std::size_t i = 1; i < m_stops.size(); ++i) {
            if (t <= m_stops[i].t) {
                const float t0 = m_stops[i - 1].t, t1 = m_stops[i].t;
                const float u = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
                return lerpColor(m_stops[i - 1].c, m_stops[i].c, u);
            }
        }
        return m_stops.back().c;
    }

private:
    struct Stop {
        float t;
        render::Color c;
    };
    std::vector<Stop> m_stops;
};

// ---- emission shape (Godot CPUParticles2D emission shapes) ------------------------------------------
struct EmitShape {
    enum Type { Point, Circle, Ring, Rect };
    int type = Point;
    float radius = 0.0f;         // Circle: filled disk radius; Ring: outer radius
    float innerRadius = 0.0f;    // Ring inner radius
    math::vec2 half{0.0f, 0.0f}; // Rect half-extents
};

// Map two uniform [0,1) samples to an emission offset from the emitter origin. Disk sampling uses
// sqrt(u) so points are area-uniform (not clustered at the centre).
inline math::vec2 sampleOffset(const EmitShape& s, float u, float v) {
    const float twoPi = 6.2831853f;
    switch (s.type) {
    case EmitShape::Circle: {
        const float r = s.radius * std::sqrt(u);
        const float a = v * twoPi;
        return math::vec2(std::cos(a) * r, std::sin(a) * r);
    }
    case EmitShape::Ring: {
        const float r = glm::mix(s.innerRadius, s.radius, u);
        const float a = v * twoPi;
        return math::vec2(std::cos(a) * r, std::sin(a) * r);
    }
    case EmitShape::Rect:
        return math::vec2((u * 2.0f - 1.0f) * s.half.x, (v * 2.0f - 1.0f) * s.half.y);
    default:
        return math::vec2(0.0f, 0.0f); // Point
    }
}

// ---- the emitter resource --------------------------------------------------------------------------
struct Emitter {
    math::vec2 position{0.0f, 0.0f};
    int count = 64;
    float duration = 1.0f;      // the `count` particles are born across this span...
    float explosiveness = 0.0f; // ...spread evenly (0) or all at t=0 (1)
    float lifeMin = 0.6f, lifeMax = 1.0f;
    EmitShape shape;
    math::vec2 direction{0.0f, -1.0f}; // base launch direction (screen -y = up)
    float spread = 0.0f;               // half-angle (radians) around direction; PI = full circle
    float speedMin = 60.0f, speedMax = 120.0f;
    math::vec2 gravity{0.0f, 200.0f}; // px/s^2 (+y = down)
    float sizeBase = 8.0f;
    Curve scale;    // per-lifetime multiplier on sizeBase (empty -> 1)
    Curve alpha;    // per-lifetime multiplier on colour alpha (empty -> 1)
    Gradient color; // per-lifetime colour (empty -> white)
};

// One simulated particle's drawable state.
struct ParticleState {
    math::vec2 pos;
    float size;
    render::Color color;
};

namespace detail {
// Deterministic per-particle random in [0,1): a hash of (seed, index, channel). No global RNG state, so
// the whole simulation is reproducible from the seed alone.
inline float hash01(uint32_t seed, int index, int channel) {
    uint64_t x = static_cast<uint64_t>(seed) * 0x9E3779B97F4A7C15ull +
                 static_cast<uint64_t>(index) * 0xD1B54A32D192ED03ull +
                 static_cast<uint64_t>(channel) * 0xCA5A826395121157ull + 0x2545F4914F6CDD1Dull;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33;
    x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return static_cast<float>(x >> 40) / static_cast<float>(1u << 24); // top 24 bits -> [0,1)
}
} // namespace detail

// Simulate the emitter deterministically and return the particles alive at global time `t`. Constant
// acceleration: pos = origin + emitOffset + vel*age + 0.5*gravity*age^2.
inline std::vector<ParticleState> simulate(const Emitter& e, uint32_t seed, float t) {
    std::vector<ParticleState> out;
    for (int i = 0; i < e.count; ++i) {
        const float frac = e.count > 1 ? static_cast<float>(i) / static_cast<float>(e.count - 1) : 0.0f;
        const float birth = (1.0f - e.explosiveness) * frac * e.duration;
        const float age = t - birth;

        const float life = glm::mix(e.lifeMin, e.lifeMax, detail::hash01(seed, i, 0));
        if (age < 0.0f || age > life) {
            continue; // not alive at this time
        }
        const math::vec2 off = sampleOffset(e.shape, detail::hash01(seed, i, 1), detail::hash01(seed, i, 2));
        const float baseAng = std::atan2(e.direction.y, e.direction.x);
        const float ang = baseAng + (detail::hash01(seed, i, 3) * 2.0f - 1.0f) * e.spread;
        const float spd = glm::mix(e.speedMin, e.speedMax, detail::hash01(seed, i, 4));
        const math::vec2 vel(std::cos(ang) * spd, std::sin(ang) * spd);
        const math::vec2 pos = e.position + off + vel * age + 0.5f * e.gravity * age * age;

        const float u = life > 0.0f ? age / life : 0.0f;
        const float sz = e.sizeBase * (e.scale.size() ? e.scale.sample(u) : 1.0f);
        render::Color col = e.color.size() ? e.color.sample(u) : render::Color{1.0f, 1.0f, 1.0f, 1.0f};
        col.a *= (e.alpha.size() ? e.alpha.sample(u) : 1.0f);
        out.push_back({pos, sz, col});
    }
    return out;
}

} // namespace maz::fx
