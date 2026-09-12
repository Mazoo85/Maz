// tests/render/equirect.cpp — verifies equirectangular panorama mapping (render Equirect.hpp).
// Ground truths, deterministic (seeded LCG for directions, no <random>, no clock):
//   * known anchor directions map to the expected UVs (+Z->0.5,0.5; +X->0.75; -X->0.25; +Y->v=0; -Y->v=1);
//   * dir -> uv -> dir round-trips exactly for thousands of random unit directions;
//   * uv -> dir -> uv round-trips away from the poles; every UV stays in [0,1]^2;
//   * bilinear sampling of a constant image returns that constant in every direction; a horizontal ramp
//     reads the expected value at anchor directions.
#include "maz/render/Equirect.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;
using maz::math::vec3;
using maz::render::Color;
using maz::render::Image;

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
    // --- 1. Anchor directions. ---
    {
        vec2 uz = maz::render::equirectUvFromDir(vec3{0, 0, 1});
        CHECK(std::fabs(uz.x - 0.5f) < 1e-4f && std::fabs(uz.y - 0.5f) < 1e-4f, "+Z -> (0.5, 0.5)");
        vec2 ux = maz::render::equirectUvFromDir(vec3{1, 0, 0});
        CHECK(std::fabs(ux.x - 0.75f) < 1e-4f && std::fabs(ux.y - 0.5f) < 1e-4f, "+X -> u=0.75");
        vec2 uxn = maz::render::equirectUvFromDir(vec3{-1, 0, 0});
        CHECK(std::fabs(uxn.x - 0.25f) < 1e-4f, "-X -> u=0.25");
        vec2 uy = maz::render::equirectUvFromDir(vec3{0, 1, 0});
        CHECK(std::fabs(uy.y - 0.0f) < 1e-4f, "+Y (up) -> v=0 (top)");
        vec2 uyn = maz::render::equirectUvFromDir(vec3{0, -1, 0});
        CHECK(std::fabs(uyn.y - 1.0f) < 1e-4f, "-Y (down) -> v=1 (bottom)");
    }

    // --- 2. dir -> uv -> dir round-trips; UV in range. ---
    {
        Lcg rng{0xE0D17u};
        bool rtOk = true, rangeOk = true;
        for (int t = 0; t < 5000 && rtOk && rangeOk; ++t) {
            vec3 d{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)};
            const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len < 1e-3f) continue;
            d = vec3{d.x / len, d.y / len, d.z / len};
            const vec2 uv = maz::render::equirectUvFromDir(d);
            if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) rangeOk = false;
            const vec3 back = maz::render::dirFromEquirectUv(uv);
            if (std::fabs(back.x - d.x) > 1e-3f || std::fabs(back.y - d.y) > 1e-3f ||
                std::fabs(back.z - d.z) > 1e-3f)
                rtOk = false;
        }
        CHECK(rangeOk, "every UV stays within [0,1]^2");
        CHECK(rtOk, "dir -> uv -> dir round-trips exactly");
    }

    // --- 3. uv -> dir -> uv round-trips away from the poles. ---
    {
        Lcg rng{0x5A5Au};
        bool ok = true;
        for (int t = 0; t < 3000 && ok; ++t) {
            const vec2 uv{rng.range(0.0f, 1.0f), rng.range(0.08f, 0.92f)}; // avoid pole rows
            const vec3 d = maz::render::dirFromEquirectUv(uv);
            const vec2 uv2 = maz::render::equirectUvFromDir(d);
            float du = std::fabs(uv2.x - uv.x);
            du = std::fmin(du, 1.0f - du); // seam wrap
            if (du > 1e-3f || std::fabs(uv2.y - uv.y) > 1e-3f) ok = false;
        }
        CHECK(ok, "uv -> dir -> uv round-trips away from the poles");
    }

    // --- 4. Sampling a constant image returns the constant everywhere. ---
    {
        Image img(64, 32, Color{0.3f, 0.7f, 0.2f, 1.0f});
        Lcg rng{0x9182u};
        bool ok = true;
        for (int t = 0; t < 200 && ok; ++t) {
            vec3 d{rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)};
            const Color c = maz::render::sampleEquirect(img, d);
            if (std::fabs(c.r - 0.3f) > 0.02f || std::fabs(c.g - 0.7f) > 0.02f || std::fabs(c.b - 0.2f) > 0.02f)
                ok = false;
        }
        CHECK(ok, "constant panorama samples to that constant in every direction");
    }

    // --- 5. Horizontal ramp: red increases with u; +X (u=0.75) is brighter than -X (u=0.25). ---
    {
        Image img(64, 32);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 64; ++x) {
                const float r = static_cast<float>(x) / 63.0f;
                img.setPixel(x, y, Color{r, 0.0f, 0.0f, 1.0f});
            }
        const Color at75 = maz::render::sampleEquirect(img, vec3{1, 0, 0});  // u=0.75
        const Color at25 = maz::render::sampleEquirect(img, vec3{-1, 0, 0}); // u=0.25
        CHECK(at75.r > at25.r + 0.3f, "ramp panorama: +X direction reads a higher u than -X");
        CHECK(std::fabs(at75.r - 0.75f) < 0.05f, "ramp value at +X matches u=0.75");
    }

    if (g_fail == 0) {
        std::printf("equirect: OK — anchors, dir<->uv round-trip, uv<->dir round-trip, constant + ramp sampling.\n");
        return 0;
    }
    std::printf("equirect: %d failure(s).\n", g_fail);
    return 1;
}
