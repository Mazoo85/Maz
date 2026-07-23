// tests/math/shapefit.cpp — verifies geometric circle/sphere fitting (math ShapeFit.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * EXACT: points sampled exactly on a known circle/sphere recover its centre and radius to ~1e-4;
//   * NOISY: with small jitter the fit stays close to the true shape, and every point lies ~radius from the
//     recovered centre (small residual);
//   * degenerate input (collinear points / coplanar points, or too few) is reported as a failed fit;
//   * determinism.
#include "maz/math/ShapeFit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec2;
using maz::math::vec3;

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
    const float pi = 3.14159265358979323846f;
    Lcg rng{0x0F17u};

    // --- 1. Circle: exact recovery. ---
    {
        const vec2 c(3.0f, -2.0f);
        const float r = 5.0f;
        std::vector<vec2> pts;
        for (int i = 0; i < 12; ++i) {
            const float t = 2.0f * pi * static_cast<float>(i) / 12.0f;
            pts.push_back(vec2(c.x + r * std::cos(t), c.y + r * std::sin(t)));
        }
        const auto f = maz::math::fitCircle(pts);
        CHECK(f.ok, "the circle fit succeeds");
        CHECK(std::fabs(f.center.x - c.x) < 1e-3f && std::fabs(f.center.y - c.y) < 1e-3f,
              "the circle centre is recovered exactly");
        CHECK(std::fabs(f.radius - r) < 1e-3f, "the circle radius is recovered exactly");
    }

    // --- 2. Circle: noisy fit stays close, small residual. ---
    {
        const vec2 c(-1.5f, 4.0f);
        const float r = 3.0f;
        std::vector<vec2> pts;
        for (int i = 0; i < 60; ++i) {
            const float t = rng.range(0.0f, 2.0f * pi);
            const float jr = r + rng.range(-0.03f, 0.03f);
            pts.push_back(vec2(c.x + jr * std::cos(t), c.y + jr * std::sin(t)));
        }
        const auto f = maz::math::fitCircle(pts);
        CHECK(f.ok && std::fabs(f.center.x - c.x) < 0.1f && std::fabs(f.center.y - c.y) < 0.1f &&
                  std::fabs(f.radius - r) < 0.1f,
              "a noisy circle is fit close to the true centre and radius");
        float maxRes = 0.0f;
        for (const vec2& p : pts) {
            const float d = std::sqrt((p.x - f.center.x) * (p.x - f.center.x) + (p.y - f.center.y) * (p.y - f.center.y));
            maxRes = std::max(maxRes, std::fabs(d - f.radius));
        }
        CHECK(maxRes < 0.1f, "every point lies within the noise band of the fitted circle");
    }

    // --- 3. Circle: degenerate inputs fail. ---
    {
        std::vector<vec2> collinear{{0, 0}, {1, 1}, {2, 2}, {3, 3}};
        CHECK(!maz::math::fitCircle(collinear).ok, "collinear points cannot fit a circle");
        std::vector<vec2> tooFew{{0, 0}, {1, 0}};
        CHECK(!maz::math::fitCircle(tooFew).ok, "fewer than three points cannot fit a circle");
    }

    // --- 4. Sphere: exact recovery. ---
    {
        const vec3 c(1.0f, 2.0f, -3.0f);
        const float r = 4.0f;
        std::vector<vec3> pts;
        // Spread points over the sphere (fibonacci-ish) to guarantee non-coplanarity.
        for (int i = 0; i < 30; ++i) {
            const float z = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / 30.0f;
            const float rr = std::sqrt(std::max(0.0f, 1.0f - z * z));
            const float phi = static_cast<float>(i) * 2.399963f;
            pts.push_back(vec3(c.x + r * rr * std::cos(phi), c.y + r * rr * std::sin(phi), c.z + r * z));
        }
        const auto f = maz::math::fitSphere(pts);
        CHECK(f.ok, "the sphere fit succeeds");
        CHECK(std::fabs(f.center.x - c.x) < 1e-3f && std::fabs(f.center.y - c.y) < 1e-3f &&
                  std::fabs(f.center.z - c.z) < 1e-3f,
              "the sphere centre is recovered exactly");
        CHECK(std::fabs(f.radius - r) < 1e-3f, "the sphere radius is recovered exactly");
    }

    // --- 5. Sphere: degenerate (coplanar) fails. ---
    {
        std::vector<vec3> coplanar{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {2, 3, 0}};
        CHECK(!maz::math::fitSphere(coplanar).ok, "coplanar points cannot fit a sphere");
    }

    // --- 6. Determinism. ---
    {
        std::vector<vec2> pts{{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {0.7f, 0.7f}};
        const auto a = maz::math::fitCircle(pts);
        const auto b = maz::math::fitCircle(pts);
        CHECK(a.center.x == b.center.x && a.center.y == b.center.y && a.radius == b.radius,
              "identical inputs produce identical fits");
    }

    if (g_fail == 0) {
        std::printf("shapefit: OK — circle exact/noisy, sphere exact, degenerate rejection, determinism.\n");
        return 0;
    }
    std::printf("shapefit: %d failure(s).\n", g_fail);
    return 1;
}
