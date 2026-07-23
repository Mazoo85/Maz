// tests/math/rbf.cpp — verifies radial-basis-function scattered interpolation (math Rbf.hpp).
// Ground truths, deterministic (seeded LCG for points/values, no <random>, no clock):
//   * the DEFINING property: the interpolant passes EXACTLY through every control point (eval(p_i)==v_i),
//     over thousands of random configurations and every kernel;
//   * a single control point produces a bump that peaks at that point and decays with distance;
//   * symmetry: two mirror-image control points with equal values give a field symmetric about the axis;
//   * determinism; degenerate (no points) evaluates to 0.
#include "maz/math/Rbf.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::RbfInterpolator2D;
using maz::math::RbfKernel;
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
    // --- 1. Interpolation property across kernels and random configs. ---
    {
        const RbfKernel kernels[3] = {RbfKernel::Gaussian, RbfKernel::Multiquadric,
                                      RbfKernel::InverseMultiquadric};
        Lcg rng{0x2B0Fu};
        bool interp = true;
        int built = 0;
        for (int t = 0; t < 1500 && interp; ++t) {
            const int n = 2 + static_cast<int>(rng.next() % 12u);
            std::vector<vec2> pts;
            std::vector<float> vals;
            for (int i = 0; i < n; ++i) {
                // Spread points on a jittered grid so they never coincide (well-posed system).
                const float gx = static_cast<float>(i % 4) * 2.0f + rng.range(-0.3f, 0.3f);
                const float gy = static_cast<float>(i / 4) * 2.0f + rng.range(-0.3f, 0.3f);
                pts.push_back(vec2{gx, gy});
                vals.push_back(rng.range(-5.0f, 5.0f));
            }
            const RbfKernel k = kernels[static_cast<std::size_t>(t) % 3u];
            const float shape = k == RbfKernel::Multiquadric ? 0.3f : 0.5f;
            RbfInterpolator2D rbf(pts, vals, k, shape);
            if (!rbf.valid()) {
                continue;
            }
            ++built;
            for (int i = 0; i < n; ++i)
                if (std::fabs(rbf.eval(pts[static_cast<std::size_t>(i)]) - vals[static_cast<std::size_t>(i)]) > 2e-3f)
                    interp = false;
        }
        CHECK(built > 1000, "the interpolation battery built valid interpolants");
        CHECK(interp, "the field passes exactly through every control point (all kernels)");
    }

    // --- 2. Single control point: bump peaks at the point, decays outward. ---
    {
        std::vector<vec2> pts{{3.0f, 3.0f}};
        std::vector<float> vals{5.0f};
        RbfInterpolator2D rbf(pts, vals, RbfKernel::Gaussian, 0.5f);
        CHECK(std::fabs(rbf.eval(vec2{3.0f, 3.0f}) - 5.0f) < 1e-4f, "single point holds its value");
        const float near = rbf.eval(vec2{3.5f, 3.0f});
        const float far = rbf.eval(vec2{6.0f, 3.0f});
        CHECK(near < 5.0f && far < near && far >= 0.0f, "Gaussian bump decays monotonically with distance");
    }

    // --- 3. Symmetry: mirrored points with equal values -> symmetric field. ---
    {
        std::vector<vec2> pts{{-2.0f, 0.0f}, {2.0f, 0.0f}};
        std::vector<float> vals{3.0f, 3.0f};
        RbfInterpolator2D rbf(pts, vals, RbfKernel::Gaussian, 0.4f);
        bool sym = true;
        for (float x = 0.0f; x <= 3.0f; x += 0.5f)
            for (float y = -2.0f; y <= 2.0f; y += 0.5f)
                if (std::fabs(rbf.eval(vec2{x, y}) - rbf.eval(vec2{-x, y})) > 1e-4f) sym = false;
        CHECK(sym, "mirror-symmetric anchors produce a field symmetric about the axis");
    }

    // --- 4. Determinism + degenerate. ---
    {
        std::vector<vec2> pts{{0.0f, 0.0f}, {1.0f, 2.0f}, {3.0f, 1.0f}};
        std::vector<float> vals{1.0f, -2.0f, 0.5f};
        RbfInterpolator2D a(pts, vals), b(pts, vals);
        CHECK(std::fabs(a.eval(vec2{1.5f, 1.0f}) - b.eval(vec2{1.5f, 1.0f})) == 0.0f, "deterministic evaluation");

        RbfInterpolator2D empty;
        CHECK(!empty.valid() && empty.eval(vec2{0.0f, 0.0f}) == 0.0f, "no points -> invalid, evaluates to 0");
    }

    if (g_fail == 0) {
        std::printf("rbf: OK — exact interpolation (all kernels), bump decay, symmetry, determinism, degenerate.\n");
        return 0;
    }
    std::printf("rbf: %d failure(s).\n", g_fail);
    return 1;
}
