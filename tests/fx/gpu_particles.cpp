// tests/fx/gpu_particles.cpp — verifies the GPU-driven particle SIMULATION + PLANE COLLISION that the
// compute shader mirrors (fx::GpuParticleSystem). All checks are pure CPU arithmetic — the same math the
// shader runs on the GPU — so the physics is provable headlessly: gravity integration, a bounce with the
// right restitution energy loss, no sinking through the floor, tangential friction, and life recycling.
#include "maz/fx/GpuParticles.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::fx;

int main() {
    // --- 1. A particle dropped onto a floor must bounce, lose energy (restitution<1), and never sink. ---
    {
        GpuParticleSystem ps(1);
        GpuParticleConfig c;
        c.originY = 5.0f;
        c.spawnJitter = 0.0f;
        c.velJitter = 0.0f;
        c.velX = c.velY = c.velZ = 0.0f;
        c.gravityY = -10.0f;
        c.restitution = 0.5f;
        c.friction = 0.0f;
        c.radius = 0.1f;
        c.lifeMin = c.lifeMax = 1000.0f; // effectively immortal for this test
        c.respawn = false;
        ps.configure(c);
        ps.addPlane(ParticlePlane{0.0f, 1.0f, 0.0f, 0.0f}); // ground: y == 0
        ps.emit(1);

        float minYSeen = 1e9f;
        float peakAfterBounce = -1e9f;
        bool bounced = false;
        float prevY = 5.0f;
        const float dt = 1.0f / 240.0f;
        for (int step = 0; step < 240 * 4; ++step) { // 4 seconds
            ps.update(dt);
            const float y = ps.positions()[1];
            minYSeen = std::min(minYSeen, y);
            const float vy = ps.velocities()[1];
            // Detect the moment velocity flips from downward to upward (a bounce just happened).
            if (!bounced && vy > 0.0f && y < 1.0f) bounced = true;
            if (bounced && y > prevY) peakAfterBounce = std::max(peakAfterBounce, y);
            prevY = y;
        }

        // Floor is at y=0, radius 0.1 -> the particle center must never go below ~0.1 (small tolerance).
        CHECK(minYSeen >= 0.1f - 1e-3f, "particle never sinks below the floor + radius");
        CHECK(bounced, "particle bounced off the floor");
        // Restitution 0.5 loses 3/4 of the kinetic energy per bounce -> peak height well under the 5.0 drop.
        CHECK(peakAfterBounce < 5.0f && peakAfterBounce > 0.0f, "bounce peak lower than drop height (energy lost)");
        // Physically ~ e^2 * height = 0.25 * ~4.9 ≈ 1.2 m. Allow a generous band.
        CHECK(peakAfterBounce < 2.5f, "bounce peak roughly matches restitution^2 * height");
    }

    // --- 2. Perfectly elastic head-on bounce (no gravity) reverses velocity, preserves speed. ---
    {
        GpuParticleSystem ps(1);
        GpuParticleConfig c;
        c.originY = 1.0f;
        c.spawnJitter = 0.0f;
        c.velJitter = 0.0f;
        c.velY = -4.0f;   // straight down
        c.gravityY = 0.0f;
        c.restitution = 1.0f;
        c.friction = 0.0f;
        c.radius = 0.0f;
        c.lifeMin = c.lifeMax = 1000.0f;
        c.respawn = false;
        ps.configure(c);
        ps.addPlane(ParticlePlane{0.0f, 1.0f, 0.0f, 0.0f});
        ps.emit(1);
        const float dt = 1.0f / 120.0f;
        for (int step = 0; step < 120; ++step) ps.update(dt); // 1 second — long enough to hit and rebound
        const float vy = ps.velocities()[1];
        CHECK(vy > 3.9f && vy < 4.1f, "elastic bounce reverses to +4 (speed preserved)");
    }

    // --- 3. Tangential friction slows horizontal sliding on impact. ---
    {
        auto slideSpeed = [](float friction) {
            GpuParticleSystem ps(1);
            GpuParticleConfig c;
            c.originY = 0.5f;
            c.spawnJitter = 0.0f;
            c.velJitter = 0.0f;
            c.velX = 5.0f;    // moving sideways
            c.velY = -3.0f;   // and into the floor
            c.gravityY = 0.0f;
            c.restitution = 0.0f;
            c.friction = friction;
            c.radius = 0.0f;
            c.lifeMin = c.lifeMax = 1000.0f;
            c.respawn = false;
            ps.configure(c);
            ps.addPlane(ParticlePlane{0.0f, 1.0f, 0.0f, 0.0f});
            ps.emit(1);
            const float dt = 1.0f / 120.0f;
            for (int step = 0; step < 60; ++step) ps.update(dt);
            return ps.velocities()[0]; // remaining horizontal speed
        };
        const float vNoFric = slideSpeed(0.0f);
        const float vFric = slideSpeed(1.0f);
        CHECK(vNoFric > 4.9f, "no friction keeps horizontal speed");
        CHECK(vFric < vNoFric - 1.0f, "friction sheds horizontal speed on impact");
    }

    // --- 4. Life ages out; respawn recycles a dead particle so `alive` stays full. ---
    {
        GpuParticleSystem ps(8);
        GpuParticleConfig c;
        c.lifeMin = c.lifeMax = 0.5f;
        c.gravityY = 0.0f;
        c.respawn = true;
        ps.configure(c);
        ps.emit(8);
        CHECK(ps.alive() == 8, "8 particles spawned");
        for (int step = 0; step < 120; ++step) ps.update(1.0f / 60.0f); // 2s — several lifetimes
        CHECK(ps.alive() == 8, "respawn keeps the pool full after lifetimes elapse");

        // With respawn off, the same pool must fully drain.
        GpuParticleConfig c2 = c;
        c2.respawn = false;
        GpuParticleSystem ps2(8);
        ps2.configure(c2);
        ps2.emit(8);
        for (int step = 0; step < 120; ++step) ps2.update(1.0f / 60.0f);
        CHECK(ps2.alive() == 0, "no-respawn pool drains to empty");
    }

    // --- 5. buildInstances emits exactly one instance per live particle, alpha fading with life. ---
    {
        GpuParticleSystem ps(4);
        GpuParticleConfig c;
        c.lifeMin = c.lifeMax = 2.0f;
        c.gravityY = 0.0f;
        c.respawn = false;
        c.radius = 0.25f;
        ps.configure(c);
        ps.emit(4);
        std::vector<ParticleInstance> inst;
        ps.buildInstances(inst);
        CHECK(inst.size() == 4, "one instance per live particle");
        CHECK(std::fabs(inst[0].size - 0.5f) < 1e-4f, "instance size == 2*radius");
        CHECK(inst[0].a > 0.99f, "fresh particle is fully opaque");
        ps.update(1.0f); // half its life
        ps.buildInstances(inst);
        CHECK(inst[0].a > 0.45f && inst[0].a < 0.55f, "alpha fades to ~0.5 at half life");
    }

    if (g_fail == 0) {
        std::printf("gpu_particles: OK — bounce/energy/no-sink/friction/lifecycle/instances verified.\n");
        return 0;
    }
    std::printf("gpu_particles: %d failure(s).\n", g_fail);
    return 1;
}
