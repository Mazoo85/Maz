// tests/math/sphericaltriangle.cpp — verifies spherical triangle area / solid angle (SphericalTriangle.hpp).
// Ground truths, deterministic (fixed + seeded-LCG vertices, no <random>, no clock):
//   * OCTANT (airtight): the triangle (X,Y,Z axes) is one eighth of the sphere, so its area is 4π/8 = π/2;
//   * GIRARD (airtight): for many random triangles the Van Oosterom–Strackee area equals the spherical
//     excess A+B+C−π computed from the interior angles by an independent method;
//   * PARTITION: eight axis-octant triangles tile the whole sphere and their areas sum to 4π;
//   * DEGENERATE: a needle-thin (near-collinear) triangle has ~0 area; a hemisphere-covering fan ≈ 2π;
//   * determinism.
#include "maz/math/SphericalTriangle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static vec3 norm(const vec3& v) { return v * (1.0f / std::sqrt(dot(v, v))); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
};

int main() {
    const float pi = 3.14159265358979324f;

    // --- 1. Octant triangle has area pi/2. ---
    {
        const float area = maz::math::sphericalTriangleArea(vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));
        CHECK(std::fabs(area - pi * 0.5f) < 1e-5f, "the axis-octant triangle has area pi/2");
    }

    // --- 2. Girard: Van Oosterom area equals the interior-angle excess, over random triangles. ---
    {
        Lcg rng{0x5A17u};
        float worst = 0.0f;
        int tested = 0;
        for (int i = 0; i < 4000; ++i) {
            const vec3 a = norm(vec3(rng.sym(), rng.sym(), rng.sym()));
            const vec3 b = norm(vec3(rng.sym(), rng.sym(), rng.sym()));
            const vec3 c = norm(vec3(rng.sym(), rng.sym(), rng.sym()));
            // Skip degenerate / near-coincident vertices where both formulas are ill-conditioned.
            if (dot(a, b) > 0.999f || dot(b, c) > 0.999f || dot(c, a) > 0.999f) {
                continue;
            }
            const float area = maz::math::sphericalTriangleArea(a, b, c);
            const float excess = maz::math::sphericalExcess(a, b, c);
            // Both are the area of the SMALLER triangle; compare directly.
            worst = std::max(worst, std::fabs(area - excess));
            ++tested;
        }
        CHECK(tested > 2000, "exercised many random triangles");
        CHECK(worst < 2e-3f, "Van Oosterom area matches Girard's spherical excess");
    }

    // --- 3. Partition: the eight axis octants tile the sphere (sum of areas = 4*pi). ---
    {
        const vec3 ax[3] = {vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)};
        float total = 0.0f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2) {
                    total += maz::math::sphericalTriangleArea(ax[0] * static_cast<float>(sx),
                                                              ax[1] * static_cast<float>(sy),
                                                              ax[2] * static_cast<float>(sz));
                }
        CHECK(std::fabs(total - 4.0f * pi) < 1e-4f, "the eight octant triangles tile the sphere (sum = 4*pi)");
    }

    // --- 4. Degenerate needle ~0; hemisphere fan ~2*pi. ---
    {
        const vec3 a(1, 0, 0);
        const vec3 b = norm(vec3(1.0f, 0.01f, 0.0f));
        const vec3 c = norm(vec3(1.0f, 0.0f, 0.01f));
        CHECK(maz::math::sphericalTriangleArea(a, b, c) < 1e-3f, "a near-collinear needle has ~0 area");
        // Upper hemisphere (z >= 0) as a polygon fan around the equator; area -> 2*pi.
        std::vector<vec3> hemi;
        const int N = 720;
        for (int i = 0; i < N; ++i) {
            const float t = 2.0f * pi * static_cast<float>(i) / static_cast<float>(N);
            hemi.push_back(vec3(std::cos(t), std::sin(t), 1e-4f)); // just above the equator
        }
        // Fan from the north pole instead: area of the cap the equatorial ring bounds.
        std::vector<vec3> capFan;
        capFan.push_back(vec3(0, 0, 1));
        for (int i = 0; i <= N; ++i) {
            const float t = 2.0f * pi * static_cast<float>(i) / static_cast<float>(N);
            capFan.push_back(norm(vec3(std::cos(t), std::sin(t), 1e-3f)));
        }
        float capArea = 0.0f;
        for (std::size_t i = 1; i + 1 < capFan.size(); ++i) {
            capArea += maz::math::sphericalTriangleArea(capFan[0], capFan[i], capFan[i + 1]);
        }
        CHECK(std::fabs(capArea - 2.0f * pi) < 1e-2f, "a fan of the upper hemisphere has area ~2*pi");
    }

    // --- 5. Determinism. ---
    {
        const vec3 a(1, 0, 0), b(0, 1, 0), c(0, 0, 1);
        CHECK(maz::math::sphericalTriangleArea(a, b, c) == maz::math::sphericalTriangleArea(a, b, c),
              "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("sphericaltriangle: OK — octant, Girard excess, sphere partition, degenerate, cap.\n");
        return 0;
    }
    std::printf("sphericaltriangle: %d failure(s).\n", g_fail);
    return 1;
}
