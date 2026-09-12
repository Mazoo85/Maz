#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// maz::fx GPU-driven 3D particle system. The particle state lives in flat, struct-of-arrays float
// buffers laid out exactly as they are uploaded to a GPU storage buffer (SSBO), and the CPU
// `update()` step mirrors — arithmetic for arithmetic — the compute shader that advances those
// particles on the GPU: integrate forces (gravity + drag), collide every particle against a set of
// world planes with restitution + friction, age each particle, and recycle dead ones from the
// emitter. Because that same math runs here on the CPU, the simulation is headlessly unit-testable
// (a particle dropped onto a floor must bounce with the right energy and must never sink through the
// surface) — while on real hardware the identical arithmetic runs in a compute dispatch and the
// surviving particles are drawn with a single INSTANCED draw call (one quad, N instances), which is
// how a modern engine reaches hundreds of thousands of particles without per-particle CPU cost.
//
// Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the SIMULATION + PLANE COLLISION below are CPU-verified
// here and are the exact arithmetic the compute shader performs. The GPU compute dispatch and the
// instanced draw are wired into the Vulkan renderer and are verified on the owner's machine — this
// headless box has no GPU, so it proves the math, not the pixels.
namespace maz::fx {

// A collision plane in Hesse form: points p with dot(n, p) == d lie on the plane; dot(n, p) > d is the
// "above" (normal) side. `n` must be unit length.
struct ParticlePlane {
    float nx = 0.0f, ny = 1.0f, nz = 0.0f;
    float d = 0.0f;
};

// Emission + dynamics parameters. All spawn positions/velocities are the base value plus a symmetric
// random jitter in [-jitter, +jitter] per axis, so a single config yields a natural spread.
struct GpuParticleConfig {
    float originX = 0.0f, originY = 0.0f, originZ = 0.0f; // emitter center
    float spawnJitter = 0.0f;                             // +/- box half-extent around the origin
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;          // base spawn velocity
    float velJitter = 0.0f;                               // +/- per-axis random velocity
    float lifeMin = 1.0f, lifeMax = 2.0f;                 // seconds
    float gravityX = 0.0f, gravityY = -9.8f, gravityZ = 0.0f;
    float drag = 0.0f;         // fraction of velocity shed per second (0 = none)
    float restitution = 0.5f;  // bounce energy retained on plane hit (0 = stick, 1 = perfectly elastic)
    float friction = 0.0f;     // tangential velocity shed on plane hit (0..1)
    float radius = 0.05f;      // particle radius, used for plane collision + instance size
    bool respawn = true;       // recycle a particle from the emitter the instant its life hits 0
};

// One instance the GPU draws: a world position, a size, and an rgba tint. Flat 8 floats — exactly an
// instance-buffer stride, so `buildInstances` output uploads straight to the instanced draw path.
struct ParticleInstance {
    float x = 0.0f, y = 0.0f, z = 0.0f, size = 0.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

class GpuParticleSystem {
public:
    explicit GpuParticleSystem(std::uint32_t capacity = 4096)
        : m_capacity(capacity),
          m_pos(static_cast<std::size_t>(capacity) * 3, 0.0f),
          m_vel(static_cast<std::size_t>(capacity) * 3, 0.0f),
          m_life(capacity, 0.0f),
          m_maxLife(capacity, 0.0f) {}

    void configure(const GpuParticleConfig& c) { m_cfg = c; }
    const GpuParticleConfig& config() const { return m_cfg; }

    void addPlane(const ParticlePlane& p) { m_planes.push_back(p); }
    void clearPlanes() { m_planes.clear(); }

    // Spawn up to `n` particles from the emitter into free slots. Returns the number actually spawned
    // (capped by remaining capacity). Round-robin cursor overwrites the oldest slot when full.
    std::uint32_t emit(std::uint32_t n) {
        std::uint32_t spawned = 0;
        for (std::uint32_t k = 0; k < n; ++k) {
            std::uint32_t i = m_cursor;
            m_cursor = (m_cursor + 1) % m_capacity;
            spawnAt(i);
            ++spawned;
            if (spawned >= m_capacity) break;
        }
        countAlive();
        return spawned;
    }

    // Advance every particle by dt — the CPU mirror of the compute shader.
    void update(float dt) {
        const GpuParticleConfig& c = m_cfg;
        const float dragF = 1.0f - c.drag * dt > 0.0f ? 1.0f - c.drag * dt : 0.0f;
        for (std::uint32_t i = 0; i < m_capacity; ++i) {
            if (m_life[i] <= 0.0f) continue;
            const std::size_t o = static_cast<std::size_t>(i) * 3;

            // Integrate velocity (gravity + drag) then position (semi-implicit Euler).
            m_vel[o + 0] = (m_vel[o + 0] + c.gravityX * dt) * dragF;
            m_vel[o + 1] = (m_vel[o + 1] + c.gravityY * dt) * dragF;
            m_vel[o + 2] = (m_vel[o + 2] + c.gravityZ * dt) * dragF;
            m_pos[o + 0] += m_vel[o + 0] * dt;
            m_pos[o + 1] += m_vel[o + 1] * dt;
            m_pos[o + 2] += m_vel[o + 2] * dt;

            collide(i);

            m_life[i] -= dt;
            if (m_life[i] <= 0.0f) {
                m_life[i] = 0.0f;
                if (c.respawn) spawnAt(i);
            }
        }
        countAlive();
    }

    // Fill `out` with one instance per live particle (alpha fades linearly with remaining life).
    void buildInstances(std::vector<ParticleInstance>& out) const {
        out.clear();
        out.reserve(m_alive);
        const float size = m_cfg.radius * 2.0f;
        for (std::uint32_t i = 0; i < m_capacity; ++i) {
            if (m_life[i] <= 0.0f) continue;
            const std::size_t o = static_cast<std::size_t>(i) * 3;
            const float frac = m_maxLife[i] > 0.0f ? m_life[i] / m_maxLife[i] : 0.0f;
            ParticleInstance inst;
            inst.x = m_pos[o + 0];
            inst.y = m_pos[o + 1];
            inst.z = m_pos[o + 2];
            inst.size = size;
            inst.a = frac;
            out.push_back(inst);
        }
    }

    std::uint32_t capacity() const { return m_capacity; }
    std::uint32_t alive() const { return m_alive; }

    // Raw SoA access for GPU upload / tests: 3 floats (xyz) per particle for pos/vel; 1 float for life.
    const std::vector<float>& positions() const { return m_pos; }
    const std::vector<float>& velocities() const { return m_vel; }
    const std::vector<float>& life() const { return m_life; }

private:
    // Deterministic splitmix64 -> [0,1). Deterministic so simulation tests are reproducible.
    float rnd() {
        m_rng += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = m_rng;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        return static_cast<float>((z >> 40) & 0xFFFFFF) / static_cast<float>(0x1000000);
    }
    float jitter(float amt) { return (rnd() * 2.0f - 1.0f) * amt; }

    void spawnAt(std::uint32_t i) {
        const GpuParticleConfig& c = m_cfg;
        const std::size_t o = static_cast<std::size_t>(i) * 3;
        m_pos[o + 0] = c.originX + jitter(c.spawnJitter);
        m_pos[o + 1] = c.originY + jitter(c.spawnJitter);
        m_pos[o + 2] = c.originZ + jitter(c.spawnJitter);
        m_vel[o + 0] = c.velX + jitter(c.velJitter);
        m_vel[o + 1] = c.velY + jitter(c.velJitter);
        m_vel[o + 2] = c.velZ + jitter(c.velJitter);
        const float life = c.lifeMin + rnd() * (c.lifeMax - c.lifeMin);
        m_life[i] = life;
        m_maxLife[i] = life;
    }

    // Resolve overlap + bounce against every plane (the compute shader loops the same way).
    void collide(std::uint32_t i) {
        const GpuParticleConfig& c = m_cfg;
        const std::size_t o = static_cast<std::size_t>(i) * 3;
        for (const ParticlePlane& pl : m_planes) {
            const float signedDist =
                pl.nx * m_pos[o + 0] + pl.ny * m_pos[o + 1] + pl.nz * m_pos[o + 2] - pl.d;
            const float penetration = c.radius - signedDist; // > 0 when the sphere crosses the plane
            if (penetration <= 0.0f) continue;

            // Push the particle back out to rest exactly on the offset surface.
            m_pos[o + 0] += pl.nx * penetration;
            m_pos[o + 1] += pl.ny * penetration;
            m_pos[o + 2] += pl.nz * penetration;

            // Reflect only the inbound normal velocity; leave the tangential part, then shed friction.
            const float vn = pl.nx * m_vel[o + 0] + pl.ny * m_vel[o + 1] + pl.nz * m_vel[o + 2];
            if (vn < 0.0f) {
                const float j = (1.0f + c.restitution) * vn;
                m_vel[o + 0] -= j * pl.nx;
                m_vel[o + 1] -= j * pl.ny;
                m_vel[o + 2] -= j * pl.nz;

                // Tangential = v - (v·n)n, computed AFTER the bounce; shed `friction` of it.
                if (c.friction > 0.0f) {
                    const float vn2 =
                        pl.nx * m_vel[o + 0] + pl.ny * m_vel[o + 1] + pl.nz * m_vel[o + 2];
                    const float tx = m_vel[o + 0] - vn2 * pl.nx;
                    const float ty = m_vel[o + 1] - vn2 * pl.ny;
                    const float tz = m_vel[o + 2] - vn2 * pl.nz;
                    m_vel[o + 0] -= c.friction * tx;
                    m_vel[o + 1] -= c.friction * ty;
                    m_vel[o + 2] -= c.friction * tz;
                }
            }
        }
    }

    void countAlive() {
        std::uint32_t n = 0;
        for (std::uint32_t i = 0; i < m_capacity; ++i)
            if (m_life[i] > 0.0f) ++n;
        m_alive = n;
    }

    std::uint32_t m_capacity;
    std::uint32_t m_alive = 0;
    std::uint32_t m_cursor = 0;
    std::vector<float> m_pos;      // xyz per particle
    std::vector<float> m_vel;      // xyz per particle
    std::vector<float> m_life;     // remaining seconds (<= 0 == dead)
    std::vector<float> m_maxLife;  // spawn life, for alpha fade
    std::vector<ParticlePlane> m_planes;
    GpuParticleConfig m_cfg;
    std::uint64_t m_rng = 0x243F6A8885A308D3ull;
};

} // namespace maz::fx
