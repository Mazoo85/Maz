// tests/math/poisson.cpp — verifies the 2D Poisson/Laplace solver (math Poisson.hpp).
// Ground truths, deterministic (fixed fields, no <random>, no clock):
//   * METHOD OF MANUFACTURED SOLUTIONS (airtight): pick a known field u*, set rhs = discrete Laplacian(u*)
//     and the border = u*; the solver must converge back to u* at every interior cell;
//   * a harmonic problem (rhs=0) with a linear boundary reproduces that exact linear field;
//   * maximum principle: with rhs=0 and a hot fixed cell, every interior value lies between the min and max
//     of the fixed values, the fixed cell is held, and the field is smooth (peak at the hot cell);
//   * the returned residual actually reaches the tolerance on a converging problem.
#include "maz/math/Poisson.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    const int W = 20, H = 18;
    const std::size_t N = static_cast<std::size_t>(W) * static_cast<std::size_t>(H);
    auto at = [](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(W) +
                                        static_cast<std::size_t>(x); };

    // --- 1. Manufactured solution. ---
    {
        // A smooth known field.
        std::vector<float> ustar(N, 0.0f);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float fx = static_cast<float>(x), fy = static_cast<float>(y);
                ustar[at(x, y)] = 0.03f * fx * fx - 0.05f * fy + 0.02f * fx * fy +
                                  std::sin(0.2f * fx) * std::cos(0.15f * fy);
            }
        // rhs = discrete Laplacian of u* at interior cells.
        std::vector<float> rhs(N, 0.0f);
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x)
                rhs[at(x, y)] = ustar[at(x - 1, y)] + ustar[at(x + 1, y)] + ustar[at(x, y - 1)] +
                                ustar[at(x, y + 1)] - 4.0f * ustar[at(x, y)];
        // Initial u: border = u*, interior = 0. Fixed = border only.
        std::vector<float> u(N, 0.0f);
        std::vector<std::uint8_t> fixed(N, 0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) {
                    u[at(x, y)] = ustar[at(x, y)];
                    fixed[at(x, y)] = 1;
                }
        const maz::math::PoissonResult r = maz::math::poissonSolve(W, H, u, rhs, fixed, 20000, 1e-5f);
        float worst = 0.0f;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x)
                worst = std::fmax(worst, std::fabs(u[at(x, y)] - ustar[at(x, y)]));
        CHECK(worst < 2e-2f, "solver converges to the manufactured solution u*");
        CHECK(r.residual < 1e-4f, "final residual reaches tolerance");
    }

    // --- 2. Harmonic problem with a linear boundary reproduces the linear field. ---
    {
        std::vector<float> u(N, 0.0f), rhs(N, 0.0f);
        std::vector<std::uint8_t> fixed(N, 0);
        auto lin = [](int x, int y) { return 1.5f + 0.7f * static_cast<float>(x) - 0.4f * static_cast<float>(y); };
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) {
                    u[at(x, y)] = lin(x, y);
                    fixed[at(x, y)] = 1;
                }
        maz::math::poissonSolve(W, H, u, rhs, fixed, 20000, 1e-6f);
        float worst = 0.0f;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x)
                worst = std::fmax(worst, std::fabs(u[at(x, y)] - lin(x, y)));
        CHECK(worst < 1e-2f, "harmonic solve reproduces the exact linear field (Laplacian of a plane is 0)");
    }

    // --- 3. Maximum principle + hot fixed cell. ---
    {
        std::vector<float> u(N, 0.0f), rhs(N, 0.0f);
        std::vector<std::uint8_t> fixed(N, 0);
        // Border fixed to 0.
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) fixed[at(x, y)] = 1;
        // Hot cell in the middle fixed to 100.
        const int hx = W / 2, hy = H / 2;
        u[at(hx, hy)] = 100.0f;
        fixed[at(hx, hy)] = 1;
        maz::math::poissonSolve(W, H, u, rhs, fixed, 20000, 1e-5f);
        bool bounded = true, smooth = true;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                const float v = u[at(x, y)];
                if (v < -1e-3f || v > 100.0f + 1e-3f) bounded = false; // between the fixed min(0) and max(100)
                if (!(x == hx && y == hy) && v > 100.0f) smooth = false;
            }
        CHECK(std::fabs(u[at(hx, hy)] - 100.0f) < 1e-4f, "the hot fixed cell holds its value");
        CHECK(bounded, "maximum principle: interior values lie between the fixed min and max");
        CHECK(smooth && u[at(hx, hy + 1)] > u[at(hx, hy + 3)],
              "field peaks at the hot cell and falls off with distance");
    }

    // --- 4. Zero everywhere. ---
    {
        std::vector<float> u(N, 0.0f), rhs(N, 0.0f);
        std::vector<std::uint8_t> fixed(N, 0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) fixed[at(x, y)] = 1;
        maz::math::poissonSolve(W, H, u, rhs, fixed, 100, 1e-6f);
        bool allZero = true;
        for (float v : u)
            if (std::fabs(v) > 1e-6f) allZero = false;
        CHECK(allZero, "zero boundary + zero source stays zero");
    }

    if (g_fail == 0) {
        std::printf("poisson: OK — manufactured solution, harmonic linear, maximum principle, zero.\n");
        return 0;
    }
    std::printf("poisson: %d failure(s).\n", g_fail);
    return 1;
}
