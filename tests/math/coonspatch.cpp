// tests/math/coonspatch.cpp — verifies the bilinearly-blended Coons patch (math CoonsPatch.hpp).
// Ground truths, deterministic (fixed curved boundaries, no <random>, no clock):
//   * BOUNDARY REPRODUCTION (airtight, the defining property): the patch edges equal the four input curves
//     exactly — P(u,0)=c0(u), P(u,1)=c1(u), P(0,v)=d0(v), P(1,v)=d1(v) for all u,v;
//   * CORNERS: the four corners match the shared curve endpoints;
//   * FLAT-QUAD LIMIT: when all four edges are straight lines between the corners, the patch reduces to the
//     bilinear interpolation of the corners;
//   * grid dimensions/ordering; determinism.
#include "maz/math/CoonsPatch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

int main() {
    // Four curved boundaries of a warped patch. They must agree at the shared corners:
    //   P00=(0,0,0), P10=(1,0,0), P01=(0,1,0), P11=(1,1,0)  (plus curvature in z / bulge).
    const vec3 P00(0, 0, 0), P10(1, 0, 0), P01(0, 1, 0), P11(1, 1, 0);
    // c0: v=0 edge, from P00 to P10, bulging up in z.
    auto c0 = [&](float u) { return vec3(u, 0.0f, 0.5f * std::sin(3.14159265f * u)); };
    // c1: v=1 edge, from P01 to P11, sagging.
    auto c1 = [&](float u) { return vec3(u, 1.0f, -0.3f * std::sin(3.14159265f * u)); };
    // d0: u=0 edge, from P00 to P01, curving in x.
    auto d0 = [&](float v) { return vec3(0.2f * std::sin(3.14159265f * v), v, 0.0f); };
    // d1: u=1 edge, from P10 to P11, curving in x the other way.
    auto d1 = [&](float v) { return vec3(1.0f - 0.2f * std::sin(3.14159265f * v), v, 0.0f); };

    // --- 1. Boundary reproduction (airtight). ---
    {
        float worst = 0.0f;
        for (int i = 0; i <= 100; ++i) {
            const float t = static_cast<float>(i) / 100.0f;
            worst = std::max(worst, len(maz::math::coonsPatchPoint(c0, c1, d0, d1, t, 0.0f) - c0(t)));
            worst = std::max(worst, len(maz::math::coonsPatchPoint(c0, c1, d0, d1, t, 1.0f) - c1(t)));
            worst = std::max(worst, len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 0.0f, t) - d0(t)));
            worst = std::max(worst, len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 1.0f, t) - d1(t)));
        }
        CHECK(worst < 1e-5f, "the patch edges reproduce the four boundary curves exactly");
    }

    // --- 2. Corners match the shared endpoints. ---
    {
        CHECK(len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 0.0f, 0.0f) - P00) < 1e-6f, "corner (0,0)=P00");
        CHECK(len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 1.0f, 0.0f) - P10) < 1e-6f, "corner (1,0)=P10");
        CHECK(len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 0.0f, 1.0f) - P01) < 1e-6f, "corner (0,1)=P01");
        CHECK(len(maz::math::coonsPatchPoint(c0, c1, d0, d1, 1.0f, 1.0f) - P11) < 1e-6f, "corner (1,1)=P11");
    }

    // --- 3. Flat-quad limit: straight edges => bilinear interpolation of the corners. ---
    {
        const vec3 q00(1, 2, 3), q10(4, 2, 1), q01(0, 5, 2), q11(5, 6, 4);
        auto sc0 = [&](float u) { return q00 * (1.0f - u) + q10 * u; };
        auto sc1 = [&](float u) { return q01 * (1.0f - u) + q11 * u; };
        auto sd0 = [&](float v) { return q00 * (1.0f - v) + q01 * v; };
        auto sd1 = [&](float v) { return q10 * (1.0f - v) + q11 * v; };
        float worst = 0.0f;
        for (int i = 0; i <= 20; ++i)
            for (int j = 0; j <= 20; ++j) {
                const float u = static_cast<float>(i) / 20.0f, v = static_cast<float>(j) / 20.0f;
                const vec3 got = maz::math::coonsPatchPoint(sc0, sc1, sd0, sd1, u, v);
                const vec3 want = q00 * ((1.0f - u) * (1.0f - v)) + q10 * (u * (1.0f - v)) +
                                  q01 * ((1.0f - u) * v) + q11 * (u * v);
                worst = std::max(worst, len(got - want));
            }
        CHECK(worst < 1e-4f, "a straight-edged patch equals the bilinear interpolation of its corners");
    }

    // --- 4. Grid dimensions/ordering + determinism. ---
    {
        const int nu = 8, nv = 5;
        const auto grid = maz::math::coonsPatchGrid(c0, c1, d0, d1, nu, nv);
        CHECK(grid.size() == static_cast<std::size_t>((nu + 1) * (nv + 1)), "grid has (nu+1)*(nv+1) points");
        CHECK(len(grid.front() - P00) < 1e-6f, "grid[0] is the (0,0) corner");
        CHECK(len(grid.back() - P11) < 1e-6f, "grid.back() is the (1,1) corner");
        // Row 0 should trace the c0 boundary.
        CHECK(len(grid[static_cast<std::size_t>(nu)] - P10) < 1e-6f, "end of first row is the (1,0) corner");
        const auto grid2 = maz::math::coonsPatchGrid(c0, c1, d0, d1, nu, nv);
        CHECK(grid == grid2, "identical inputs produce identical grids");
    }

    if (g_fail == 0) {
        std::printf("coonspatch: OK — boundary reproduction, corners, flat-quad limit, grid, determinism.\n");
        return 0;
    }
    std::printf("coonspatch: %d failure(s).\n", g_fail);
    return 1;
}
