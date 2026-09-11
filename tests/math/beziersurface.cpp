// tests/math/beziersurface.cpp — verifies the bicubic Bézier surface patch (math BezierSurface.hpp).
// Ground truths, deterministic (fixed control nets, no <random>, no clock):
//   * CORNER INTERPOLATION (airtight): the patch passes exactly through its four corner control points;
//   * BOUNDARY CURVES (airtight): each patch edge equals the cubic Bézier of that edge's four control points;
//   * LINEAR PRECISION (airtight): a control net sampling an affine map base+u*U+v*V reproduces that map
//     exactly — the Bernstein basis has degree-1 precision;
//   * PLANARITY: a control net lying in a plane produces a patch that stays in that plane;
//   * NORMAL: the analytic normal is unit and perpendicular to both surface tangents; grid dims; determinism.
#include "maz/math/BezierSurface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Independent scalar cubic Bézier of four control values.
static vec3 cubicBezier(const vec3& a, const vec3& b, const vec3& c, const vec3& d, float t) {
    const float mt = 1.0f - t;
    return a * (mt * mt * mt) + b * (3.0f * t * mt * mt) + c * (3.0f * t * t * mt) + d * (t * t * t);
}

int main() {
    // A generic, non-planar control net (row-major, index = i*4 + j).
    std::array<vec3, 16> cp;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const float fi = static_cast<float>(i), fj = static_cast<float>(j);
            cp[static_cast<std::size_t>(i * 4 + j)] =
                vec3(fi, fj, 0.4f * std::sin(fi * 1.3f) * std::cos(fj * 0.9f)); // bumpy z
        }

    // --- 1. Corner interpolation. ---
    {
        CHECK(len(maz::math::bezierSurfacePoint(cp, 0, 0) - cp[0]) < 1e-5f, "P(0,0) is the first corner");
        CHECK(len(maz::math::bezierSurfacePoint(cp, 1, 0) - cp[12]) < 1e-5f, "P(1,0) is control point i=3,j=0");
        CHECK(len(maz::math::bezierSurfacePoint(cp, 0, 1) - cp[3]) < 1e-5f, "P(0,1) is control point i=0,j=3");
        CHECK(len(maz::math::bezierSurfacePoint(cp, 1, 1) - cp[15]) < 1e-5f, "P(1,1) is the last corner");
    }

    // --- 2. Boundary curves are the cubic Béziers of the edge control points. ---
    {
        float worst = 0.0f;
        for (int k = 0; k <= 40; ++k) {
            const float t = static_cast<float>(k) / 40.0f;
            // v=0 edge: control points i=0..3, j=0 -> indices 0,4,8,12.
            worst = std::max(worst, len(maz::math::bezierSurfacePoint(cp, t, 0.0f) -
                                        cubicBezier(cp[0], cp[4], cp[8], cp[12], t)));
            // u=0 edge: control points i=0, j=0..3 -> indices 0,1,2,3.
            worst = std::max(worst, len(maz::math::bezierSurfacePoint(cp, 0.0f, t) -
                                        cubicBezier(cp[0], cp[1], cp[2], cp[3], t)));
            // v=1 edge: indices 3,7,11,15.
            worst = std::max(worst, len(maz::math::bezierSurfacePoint(cp, t, 1.0f) -
                                        cubicBezier(cp[3], cp[7], cp[11], cp[15], t)));
        }
        CHECK(worst < 1e-4f, "each boundary edge equals the cubic Bezier of its control points");
    }

    // --- 3. Linear precision: an affine control net reproduces base + u*U + v*V. ---
    {
        const vec3 base(1, 2, 3), U(4, -1, 0.5f), V(-2, 3, 1);
        std::array<vec3, 16> lin;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                lin[static_cast<std::size_t>(i * 4 + j)] =
                    base + U * (static_cast<float>(i) / 3.0f) + V * (static_cast<float>(j) / 3.0f);
        float worst = 0.0f;
        for (int a = 0; a <= 10; ++a)
            for (int b = 0; b <= 10; ++b) {
                const float u = static_cast<float>(a) / 10.0f, v = static_cast<float>(b) / 10.0f;
                const vec3 want = base + U * u + V * v;
                worst = std::max(worst, len(maz::math::bezierSurfacePoint(lin, u, v) - want));
            }
        CHECK(worst < 1e-4f, "an affine control net reproduces the affine map exactly");
    }

    // --- 4. Planarity: a control net in the z=5 plane stays in that plane. ---
    {
        std::array<vec3, 16> flat;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                flat[static_cast<std::size_t>(i * 4 + j)] = vec3(static_cast<float>(i) + 0.3f * static_cast<float>(j),
                                                                 static_cast<float>(j) - 0.2f * static_cast<float>(i), 5.0f);
        float worst = 0.0f;
        for (int a = 0; a <= 15; ++a)
            for (int b = 0; b <= 15; ++b)
                worst = std::max(worst, std::fabs(maz::math::bezierSurfacePoint(flat, static_cast<float>(a) / 15.0f,
                                                                                static_cast<float>(b) / 15.0f).z - 5.0f));
        CHECK(worst < 1e-4f, "a planar control net yields a planar patch");
    }

    // --- 5. Normal is unit and perpendicular to both tangents. ---
    {
        float worstDot = 0.0f, worstLen = 0.0f;
        for (int a = 1; a < 10; ++a)
            for (int b = 1; b < 10; ++b) {
                const float u = static_cast<float>(a) / 10.0f, v = static_cast<float>(b) / 10.0f;
                vec3 du, dv;
                maz::math::bezierSurfaceTangents(cp, u, v, du, dv);
                const vec3 n = maz::math::bezierSurfaceNormal(cp, u, v);
                worstLen = std::max(worstLen, std::fabs(len(n) - 1.0f));
                worstDot = std::max(worstDot, std::fabs(dot3(n, du)) + std::fabs(dot3(n, dv)));
            }
        CHECK(worstLen < 1e-4f, "the surface normal is unit length");
        CHECK(worstDot < 1e-3f, "the normal is perpendicular to both surface tangents");
    }

    // --- 6. Grid dims + determinism. ---
    {
        const auto grid = maz::math::bezierSurfaceGrid(cp, 6, 4);
        CHECK(grid.size() == 7 * 5, "grid has (nu+1)*(nv+1) points");
        CHECK(len(grid.front() - cp[0]) < 1e-5f, "grid[0] is the P(0,0) corner");
        CHECK(len(grid.back() - cp[15]) < 1e-5f, "grid.back() is the P(1,1) corner");
        CHECK(maz::math::bezierSurfaceGrid(cp, 6, 4) == maz::math::bezierSurfaceGrid(cp, 6, 4),
              "identical inputs produce identical grids");
    }

    if (g_fail == 0) {
        std::printf("beziersurface: OK — corners, boundary curves, linear precision, planarity, normal.\n");
        return 0;
    }
    std::printf("beziersurface: %d failure(s).\n", g_fail);
    return 1;
}
