// tests/math/grid3dsample.cpp — verifies 3D (trilinear) volume sampling (math Grid3DSample.hpp).
// Ground truths, deterministic (fixed + seeded-LCG grids/coords, no <random>, no clock):
//   * NODE REPRODUCTION (airtight): sampling at integer coordinates returns the exact stored cell value;
//   * AFFINE EXACTNESS (airtight): a grid filled from f(x,y,z)=ax+by+cz+e is reproduced exactly at any
//     fractional interior coordinate (trilinear is exact on affine fields);
//   * MULTILINEAR EXACTNESS (airtight): a grid filled from a full trilinear polynomial (…+xy+xz+yz+xyz) is
//     reproduced exactly — trilinear interpolation of trilinear samples is exact;
//   * NESTED-LERP EQUIVALENCE: matches a hand-written 3-stage lerp;
//   * EDGE MODES: Clamp repeats the border, Wrap tiles; nearest snaps to the closest cell;
//   * TEMPLATED: samples a vec3 grid componentwise.
#include "maz/math/Grid3DSample.hpp"
#include "maz/math/Math.hpp" // vec3

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

int main() {
    using namespace maz::math;
    const int W = 5, H = 4, D = 3;
    auto idx = [&](int x, int y, int z) {
        return (static_cast<std::size_t>(z) * static_cast<std::size_t>(H) + static_cast<std::size_t>(y)) *
                   static_cast<std::size_t>(W) + static_cast<std::size_t>(x);
    };

    // --- 1. Node reproduction + affine exactness. ---
    {
        const float a = 1.3f, b = -0.7f, c = 2.1f, e = 0.4f;
        std::vector<float> g(static_cast<std::size_t>(W * H * D));
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    g[idx(x, y, z)] = a * static_cast<float>(x) + b * static_cast<float>(y) +
                                      c * static_cast<float>(z) + e;
        // Node reproduction.
        float worstNode = 0.0f;
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    worstNode = std::max(worstNode, std::fabs(grid3DTrilinear(g, W, H, D, static_cast<float>(x),
                                                                             static_cast<float>(y), static_cast<float>(z)) -
                                                             g[idx(x, y, z)]));
        CHECK(worstNode < 1e-5f, "trilinear at integer coords returns the stored cell value");
        // Affine exactness at random interior fractional coords.
        Lcg rng{0x6D30u};
        float worstAff = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            const float x = rng.unit() * static_cast<float>(W - 1);
            const float y = rng.unit() * static_cast<float>(H - 1);
            const float z = rng.unit() * static_cast<float>(D - 1);
            const float want = a * x + b * y + c * z + e;
            worstAff = std::max(worstAff, std::fabs(grid3DTrilinear(g, W, H, D, x, y, z) - want));
        }
        CHECK(worstAff < 1e-4f, "trilinear reproduces an affine field exactly");
    }

    // --- 2. Multilinear exactness: full trilinear polynomial. ---
    {
        auto f = [](float x, float y, float z) {
            return 0.5f + 1.1f * x - 0.6f * y + 0.3f * z + 0.7f * x * y - 0.4f * x * z + 0.2f * y * z +
                   0.15f * x * y * z;
        };
        std::vector<float> g(static_cast<std::size_t>(W * H * D));
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    g[idx(x, y, z)] = f(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        Lcg rng{0x9AA1u};
        float worst = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            // Stay within a single cell so the field equals ONE trilinear patch (exact).
            const int cx = static_cast<int>(rng.unit() * static_cast<float>(W - 1));
            const int cy = static_cast<int>(rng.unit() * static_cast<float>(H - 1));
            const int cz = static_cast<int>(rng.unit() * static_cast<float>(D - 1));
            const float x = static_cast<float>(cx) + rng.unit();
            const float y = static_cast<float>(cy) + rng.unit();
            const float z = static_cast<float>(cz) + rng.unit();
            worst = std::max(worst, std::fabs(grid3DTrilinear(g, W, H, D, x, y, z) - f(x, y, z)));
        }
        CHECK(worst < 1e-4f, "trilinear reproduces a full trilinear polynomial within each cell");
    }

    // --- 3. Nested-lerp equivalence + nearest. ---
    {
        Lcg rng{0xC0DEu};
        std::vector<float> g(static_cast<std::size_t>(W * H * D));
        for (auto& v : g) {
            v = rng.unit() * 10.0f;
        }
        auto lerp = [](float p, float q, float t) { return p + (q - p) * t; };
        float worst = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            const float x = rng.unit() * static_cast<float>(W - 1);
            const float y = rng.unit() * static_cast<float>(H - 1);
            const float z = rng.unit() * static_cast<float>(D - 1);
            const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y)), z0 = static_cast<int>(std::floor(z));
            const int x1 = x0 + 1 < W ? x0 + 1 : x0, y1 = y0 + 1 < H ? y0 + 1 : y0, z1 = z0 + 1 < D ? z0 + 1 : z0;
            const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0), fz = z - static_cast<float>(z0);
            const float c00 = lerp(g[idx(x0, y0, z0)], g[idx(x1, y0, z0)], fx);
            const float c10 = lerp(g[idx(x0, y1, z0)], g[idx(x1, y1, z0)], fx);
            const float c01 = lerp(g[idx(x0, y0, z1)], g[idx(x1, y0, z1)], fx);
            const float c11 = lerp(g[idx(x0, y1, z1)], g[idx(x1, y1, z1)], fx);
            const float want = lerp(lerp(c00, c10, fy), lerp(c01, c11, fy), fz);
            worst = std::max(worst, std::fabs(grid3DTrilinear(g, W, H, D, x, y, z) - want));
        }
        CHECK(worst < 1e-5f, "trilinear matches an independent nested 3-stage lerp");
        CHECK(grid3DNearest(g, W, H, D, 2.4f, 1.6f, 0.9f) == g[idx(2, 2, 1)], "nearest snaps to the closest cell");
    }

    // --- 4. Edge modes: Clamp repeats the border, Wrap tiles. ---
    {
        std::vector<float> g(static_cast<std::size_t>(W * H * D));
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    g[idx(x, y, z)] = static_cast<float>(idx(x, y, z));
        // Clamp: sampling past the max edge equals the border cell.
        CHECK(std::fabs(grid3DTrilinear(g, W, H, D, 100.0f, 100.0f, 100.0f, GridEdge::Clamp) -
                        g[idx(W - 1, H - 1, D - 1)]) < 1e-4f, "Clamp past the corner returns the corner cell");
        CHECK(std::fabs(grid3DTrilinear(g, W, H, D, -5.0f, -5.0f, -5.0f, GridEdge::Clamp) - g[idx(0, 0, 0)]) < 1e-4f,
              "Clamp before the origin returns the first cell");
        // Wrap: integer coord x = W equals x = 0.
        CHECK(std::fabs(grid3DTrilinear(g, W, H, D, static_cast<float>(W), 1.0f, 1.0f, GridEdge::Wrap) -
                        g[idx(0, 1, 1)]) < 1e-4f, "Wrap folds x=W back to x=0");
    }

    // --- 5. Templated vec3 grid: sampled componentwise. ---
    {
        std::vector<vec3> g(static_cast<std::size_t>(W * H * D));
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    g[idx(x, y, z)] = vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        const vec3 s = grid3DTrilinear(g, W, H, D, 1.5f, 2.25f, 0.5f);
        CHECK(std::fabs(s.x - 1.5f) < 1e-4f && std::fabs(s.y - 2.25f) < 1e-4f && std::fabs(s.z - 0.5f) < 1e-4f,
              "a vec3 field of the coordinates reproduces the coordinate exactly");
    }

    if (g_fail == 0) {
        std::printf("grid3dsample: OK — node reproduction, affine/multilinear exactness, nested lerp, edges, vec3.\n");
        return 0;
    }
    std::printf("grid3dsample: %d failure(s).\n", g_fail);
    return 1;
}
