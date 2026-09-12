// tests/math/sampling.cpp — verifies the Monte-Carlo sampling warps (math Sampling.hpp).
// Ground truths, deterministic (seeded-LCG uniforms, no <random>, no clock):
//   * DOMAIN (airtight): disk samples lie in the unit disk; triangle barycentrics are >=0 with sum <=1;
//     sphere/hemisphere directions are unit length with the right z sign;
//   * UNIFORMITY (airtight, known expectations): a uniform disk has E[r]=2/3 and half its area within
//     radius 1/sqrt(2); a uniform triangle has E[b0]=E[b1]=1/3; a uniform sphere has E[z]=0; a uniform
//     hemisphere has E[z]=1/2; a cosine-weighted hemisphere has E[z]=2/3 (the diffuse importance weight);
//   * CONCENTRIC map corners land on the disk rim and the centre stays centred;
//   * BALL (solid sphere) samples stay inside the unit sphere, are VOLUME-uniform (1/8 within radius 1/2, the
//     cube-root-warp signature that a naive radius=u would fail), centred, and deterministic.
#include "maz/math/Sampling.hpp"

#include <algorithm> // std::max — not guaranteed by <cmath>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len2(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static float len3(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 32); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; } // full [0,1)
};

int main() {
    using namespace maz::math;
    const int N = 400000;

    // --- 1. Concentric disk: domain, uniformity, corners. ---
    {
        Lcg rng{0x5A11u};
        double sumR = 0.0;
        int inner = 0, worstOut = 0;
        for (int i = 0; i < N; ++i) {
            const vec2 p = sampleConcentricDisk(rng.unit(), rng.unit());
            const float r = len2(p);
            if (r > 1.0f + 1e-4f) {
                ++worstOut;
            }
            sumR += static_cast<double>(r);
            if (r <= 0.70710678f) { // 1/sqrt(2): the median radius of a uniform disk (half the area)
                ++inner;
            }
        }
        CHECK(worstOut == 0, "all concentric-disk samples lie in the unit disk");
        CHECK(std::fabs(sumR / N - 2.0 / 3.0) < 5e-3, "uniform disk mean radius is 2/3");
        CHECK(std::fabs(static_cast<double>(inner) / N - 0.5) < 5e-3, "half the disk area lies within r=1/sqrt(2)");
        // Corners of the unit square map to the rim; centre to the centre.
        CHECK(std::fabs(len2(sampleConcentricDisk(1.0f, 0.5f)) - 1.0f) < 1e-4f, "a square edge maps to the disk rim");
        CHECK(len2(sampleConcentricDisk(0.5f, 0.5f)) < 1e-4f, "the square centre maps to the disk centre");
    }

    // --- 2. Uniform disk (sqrt map): domain + mean radius. ---
    {
        Lcg rng{0xD152u};
        double sumR = 0.0;
        int worstOut = 0;
        for (int i = 0; i < N; ++i) {
            const vec2 p = sampleUniformDisk(rng.unit(), rng.unit());
            const float r = len2(p);
            if (r > 1.0f + 1e-4f) {
                ++worstOut;
            }
            sumR += static_cast<double>(r);
        }
        CHECK(worstOut == 0, "all sqrt-map disk samples lie in the unit disk");
        CHECK(std::fabs(sumR / N - 2.0 / 3.0) < 5e-3, "sqrt-map disk mean radius is 2/3");
    }

    // --- 3. Uniform triangle barycentrics. ---
    {
        Lcg rng{0x7213u};
        double sb0 = 0.0, sb1 = 0.0;
        int bad = 0;
        for (int i = 0; i < N; ++i) {
            const vec2 b = sampleUniformTriangle(rng.unit(), rng.unit());
            if (b.x < -1e-5f || b.y < -1e-5f || b.x + b.y > 1.0f + 1e-4f) {
                ++bad;
            }
            sb0 += static_cast<double>(b.x);
            sb1 += static_cast<double>(b.y);
        }
        CHECK(bad == 0, "triangle barycentrics stay inside the triangle");
        CHECK(std::fabs(sb0 / N - 1.0 / 3.0) < 5e-3 && std::fabs(sb1 / N - 1.0 / 3.0) < 5e-3,
              "uniform triangle centroid mean is (1/3, 1/3)");
    }

    // --- 4. Cosine hemisphere: unit, z>=0, E[z]=2/3. ---
    {
        Lcg rng{0xC051u};
        double sumZ = 0.0, worstLen = 0.0;
        int badZ = 0;
        for (int i = 0; i < N; ++i) {
            const vec3 d = sampleCosineHemisphere(rng.unit(), rng.unit());
            worstLen = std::max(worstLen, std::fabs(static_cast<double>(len3(d)) - 1.0));
            if (d.z < -1e-5f) {
                ++badZ;
            }
            sumZ += static_cast<double>(d.z);
        }
        CHECK(badZ == 0 && worstLen < 1e-4, "cosine-hemisphere samples are unit vectors on the +Z hemisphere");
        CHECK(std::fabs(sumZ / N - 2.0 / 3.0) < 5e-3, "cosine-weighted E[z] is 2/3");
    }

    // --- 5. Uniform hemisphere E[z]=1/2 and uniform sphere E[z]=0. ---
    {
        Lcg rng{0x0FF5u};
        double sHemi = 0.0, sSph = 0.0;
        int bad = 0;
        double worstLen = 0.0;
        for (int i = 0; i < N; ++i) {
            const vec3 h = sampleUniformHemisphere(rng.unit(), rng.unit());
            const vec3 s = sampleUniformSphere(rng.unit(), rng.unit());
            if (h.z < -1e-5f) {
                ++bad;
            }
            worstLen = std::max(worstLen, std::fabs(static_cast<double>(len3(h)) - 1.0));
            worstLen = std::max(worstLen, std::fabs(static_cast<double>(len3(s)) - 1.0));
            sHemi += static_cast<double>(h.z);
            sSph += static_cast<double>(s.z);
        }
        CHECK(bad == 0 && worstLen < 1e-4, "hemisphere/sphere samples are unit vectors (hemisphere +Z)");
        CHECK(std::fabs(sHemi / N - 0.5) < 5e-3, "uniform-hemisphere E[z] is 1/2");
        CHECK(std::fabs(sSph / N) < 5e-3, "uniform-sphere E[z] is 0");
    }

    // --- 6. Uniform ball (solid sphere interior): domain, volume-uniformity, centred mean, determinism. ---
    {
        Lcg rng{0xBA11u};
        double sumX = 0.0, sumY = 0.0, sumZ = 0.0;
        int worstOut = 0, withinHalf = 0;
        for (int i = 0; i < N; ++i) {
            const vec3 p = sampleUniformBall(rng.unit(), rng.unit(), rng.unit());
            const float r = len3(p);
            if (r > 1.0f + 1e-4f) {
                ++worstOut;
            }
            if (r <= 0.5f) { // fraction within radius 1/2 should be (1/2)^3 = 1/8 for a VOLUME-uniform ball
                ++withinHalf;
            }
            sumX += static_cast<double>(p.x);
            sumY += static_cast<double>(p.y);
            sumZ += static_cast<double>(p.z);
        }
        CHECK(worstOut == 0, "all ball samples lie inside the unit sphere");
        // The discriminator: a naive radius=u warp would put ~1/2 of points within r=1/2; the cbrt warp puts 1/8.
        CHECK(std::fabs(static_cast<double>(withinHalf) / N - 0.125) < 5e-3, "ball is volume-uniform (1/8 within r=1/2)");
        CHECK(std::fabs(sumX / N) < 5e-3 && std::fabs(sumY / N) < 5e-3 && std::fabs(sumZ / N) < 5e-3,
              "uniform ball is centred at the origin");
        // Endpoints and determinism.
        CHECK(len3(sampleUniformBall(0.5f, 0.5f, 0.0f)) < 1e-4f, "u3=0 maps to the centre");
        CHECK(std::fabs(len3(sampleUniformBall(0.3f, 0.7f, 1.0f)) - 1.0f) < 1e-4f, "u3=1 maps to the surface");
        const vec3 a = sampleUniformBall(0.11f, 0.22f, 0.33f);
        const vec3 b = sampleUniformBall(0.11f, 0.22f, 0.33f);
        CHECK(a.x == b.x && a.y == b.y && a.z == b.z, "sampleUniformBall is deterministic");
    }

    if (g_fail == 0) {
        std::printf("sampling: OK — disk/triangle/hemisphere/sphere warps + volume-uniform ball.\n");
        return 0;
    }
    std::printf("sampling: %d failure(s).\n", g_fail);
    return 1;
}
