// tests/game/sph2d.cpp — verifies 2D SPH fluid density + forces (game Sph2D.hpp).
// Ground truths, deterministic (seeded LCG for particle clouds, no <random>, no clock):
//   * density is positive and a lone particle has exactly its self-contribution;
//   * closer neighbours raise density (compression is felt);
//   * MOMENTUM: with gravity and viscosity off, the symmetric pressure forces conserve momentum exactly —
//     sum of mass*acceleration is ~0 over random clouds;
//   * an over-compressed pair repels (acceleration pushes the two apart);
//   * viscosity opposes relative motion; gravity adds uniformly; determinism.
#include "maz/game/Sph2D.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::SphParams;
using maz::math::vec2;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

int main() {
    SphParams p;
    p.h = 1.0f;
    p.restDensity = 1.0f;
    p.gasK = 2.0f;
    p.mass = 1.0f;

    // --- 1. Lone particle density = self contribution; density positive. ---
    {
        std::vector<vec2> one{{0.0f, 0.0f}};
        const std::vector<float> rho = maz::game::sphDensities(one, p);
        const float c6 = 315.0f / (64.0f * 3.14159265358979323846f * std::pow(p.h, 9.0f));
        const float self = p.mass * c6 * std::pow(p.h * p.h, 3.0f);
        CHECK(std::fabs(rho[0] - self) < 1e-4f && rho[0] > 0.0f, "lone particle density is its self contribution");
    }

    // --- 2. Closer neighbour raises density. ---
    {
        std::vector<vec2> far{{0.0f, 0.0f}, {0.9f, 0.0f}};
        std::vector<vec2> near{{0.0f, 0.0f}, {0.3f, 0.0f}};
        CHECK(maz::game::sphDensities(near, p)[0] > maz::game::sphDensities(far, p)[0],
              "a closer neighbour increases density");
    }

    // --- 3. Momentum conservation of pressure forces (no gravity/viscosity). ---
    {
        Lcg rng{0x59A1u};
        bool ok = true;
        int trials = 0;
        for (int t = 0; t < 200 && ok; ++t) {
            const int n = 4 + static_cast<int>(rng.next() % 20u);
            std::vector<vec2> pos;
            std::vector<vec2> vel(static_cast<std::size_t>(n), vec2{0.0f, 0.0f});
            for (int i = 0; i < n; ++i) pos.push_back(vec2{rng.range(-1.5f, 1.5f), rng.range(-1.5f, 1.5f)});
            const std::vector<float> rho = maz::game::sphDensities(pos, p);
            const std::vector<vec2> acc = maz::game::sphAccelerations(pos, vel, rho, p);
            vec2 total{0.0f, 0.0f};
            float scale = 0.0f;
            for (int i = 0; i < n; ++i) {
                total.x += p.mass * acc[static_cast<std::size_t>(i)].x;
                total.y += p.mass * acc[static_cast<std::size_t>(i)].y;
                scale += p.mass * std::sqrt(acc[static_cast<std::size_t>(i)].x * acc[static_cast<std::size_t>(i)].x +
                                            acc[static_cast<std::size_t>(i)].y * acc[static_cast<std::size_t>(i)].y);
            }
            const float rel = std::sqrt(total.x * total.x + total.y * total.y) / (scale + 1e-9f);
            if (rel > 1e-3f) ok = false;
            ++trials;
        }
        CHECK(trials > 100 && ok, "symmetric pressure forces conserve momentum (sum of mass*accel ~ 0)");
    }

    // --- 4. Over-compressed pair repels. ---
    {
        std::vector<vec2> pos{{0.0f, 0.0f}, {0.35f, 0.0f}}; // very close -> high density -> pressure
        std::vector<vec2> vel(2, vec2{0.0f, 0.0f});
        const std::vector<float> rho = maz::game::sphDensities(pos, p);
        const std::vector<vec2> acc = maz::game::sphAccelerations(pos, vel, rho, p);
        // Particle 0 should accelerate in -x (away from particle 1 at +x); particle 1 in +x.
        CHECK(acc[0].x < 0.0f && acc[1].x > 0.0f, "an over-compressed pair pushes apart");
    }

    // --- 5. Viscosity opposes relative velocity. ---
    {
        SphParams pv = p;
        pv.viscosity = 1.0f;
        std::vector<vec2> pos{{0.0f, 0.0f}, {0.5f, 0.0f}};
        std::vector<vec2> vel{{0.0f, 1.0f}, {0.0f, -1.0f}}; // moving apart in y
        const std::vector<float> rho = maz::game::sphDensities(pos, pv);
        const std::vector<vec2> acc = maz::game::sphAccelerations(pos, vel, rho, pv);
        // Viscosity should pull each toward the other's velocity: particle 0 (vy=+1) gets a_y < 0.
        CHECK(acc[0].y < 0.0f && acc[1].y > 0.0f, "viscosity damps relative velocity");
    }

    // --- 6. Gravity adds uniformly; determinism. ---
    {
        SphParams pg = p;
        pg.gravity = vec2{0.0f, -9.8f};
        std::vector<vec2> pos{{0.0f, 0.0f}, {5.0f, 5.0f}}; // far apart -> no interaction
        std::vector<vec2> vel(2, vec2{0.0f, 0.0f});
        const std::vector<float> rho = maz::game::sphDensities(pos, pg);
        const std::vector<vec2> acc = maz::game::sphAccelerations(pos, vel, rho, pg);
        CHECK(std::fabs(acc[0].y + 9.8f) < 1e-3f && std::fabs(acc[1].y + 9.8f) < 1e-3f, "gravity adds uniformly");
        const std::vector<vec2> acc2 = maz::game::sphAccelerations(pos, vel, rho, pg);
        CHECK(acc[0].x == acc2[0].x && acc[0].y == acc2[0].y, "deterministic");
    }

    if (g_fail == 0) {
        std::printf("sph2d: OK — density, compression, momentum conservation, repulsion, viscosity, gravity.\n");
        return 0;
    }
    std::printf("sph2d: %d failure(s).\n", g_fail);
    return 1;
}
