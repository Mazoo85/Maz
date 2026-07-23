// tests/math/linearsolve.cpp — verifies the dense LU linear solver (math LinearSolve.hpp).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * a known small system solves exactly;
//   * ROUND-TRIP: for random A and x, solving A(Ax)=b recovers x, and the residual A*x-b ~ 0;
//   * identity: solve(I, b) == b;
//   * a singular matrix is reported as unsolvable, and its determinant is 0;
//   * determinant matches the analytic 2x2 (ad-bc) and a diagonal product;
//   * inverse: A * inverse(A) == I;
//   * determinism.
#include "maz/math/LinearSolve.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    double range(double lo, double hi) { return lo + (hi - lo) * (static_cast<double>(next() % 1000000u) / 999999.0); }
};

static double residual(const std::vector<double>& A, const std::vector<double>& x, const std::vector<double>& b, int n) {
    double r = 0.0;
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += A[static_cast<std::size_t>(i * n + j)] * x[static_cast<std::size_t>(j)];
        r = std::max(r, std::fabs(s - b[static_cast<std::size_t>(i)]));
    }
    return r;
}

int main() {
    Lcg rng{0x50A1u};

    // --- 1. Known 3x3 system. ---
    {
        // [2 1 -1; -3 -1 2; -2 1 2] x = [8; -11; -3]  ->  x = [2; 3; -1]
        std::vector<double> A{2, 1, -1, -3, -1, 2, -2, 1, 2};
        std::vector<double> b{8, -11, -3};
        std::vector<double> x;
        CHECK(maz::math::solveLinearSystem(A, b, 3, x), "the known system is solvable");
        CHECK(std::fabs(x[0] - 2) < 1e-9 && std::fabs(x[1] - 3) < 1e-9 && std::fabs(x[2] + 1) < 1e-9,
              "the known 3x3 system solves to the expected vector");
    }

    // --- 2. Round-trip on random diagonally-dominant systems. ---
    {
        bool ok = true;
        int trials = 0;
        for (int t = 0; t < 200; ++t) {
            const int n = 2 + static_cast<int>(rng.next() % 7u); // 2..8
            std::vector<double> A(static_cast<std::size_t>(n * n));
            std::vector<double> xTrue(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                xTrue[static_cast<std::size_t>(i)] = rng.range(-5, 5);
                for (int j = 0; j < n; ++j) A[static_cast<std::size_t>(i * n + j)] = rng.range(-2, 2);
                A[static_cast<std::size_t>(i * n + i)] += static_cast<double>(n) + 3.0; // diagonally dominant -> invertible
            }
            std::vector<double> b(static_cast<std::size_t>(n), 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) b[static_cast<std::size_t>(i)] += A[static_cast<std::size_t>(i * n + j)] * xTrue[static_cast<std::size_t>(j)];
            std::vector<double> x;
            if (!maz::math::solveLinearSystem(A, b, n, x)) { ok = false; continue; }
            double err = 0.0;
            for (int i = 0; i < n; ++i) err = std::max(err, std::fabs(x[static_cast<std::size_t>(i)] - xTrue[static_cast<std::size_t>(i)]));
            if (err > 1e-6 || residual(A, x, b, n) > 1e-6) ok = false;
            ++trials;
        }
        CHECK(trials > 150 && ok, "solving A(Ax)=b recovers x with a tiny residual over random systems");
    }

    // --- 3. Identity. ---
    {
        std::vector<double> I{1, 0, 0, 0, 1, 0, 0, 0, 1};
        std::vector<double> b{4, -2, 7};
        std::vector<double> x;
        maz::math::solveLinearSystem(I, b, 3, x);
        CHECK(x[0] == 4 && x[1] == -2 && x[2] == 7, "solving the identity returns b unchanged");
    }

    // --- 4. Singular detection + determinant 0. ---
    {
        std::vector<double> S{1, 2, 3, 2, 4, 6, 1, 1, 1}; // row2 = 2*row1 -> singular
        std::vector<double> b{1, 2, 3};
        std::vector<double> x;
        CHECK(!maz::math::solveLinearSystem(S, b, 3, x), "a singular system is reported unsolvable");
        CHECK(std::fabs(maz::math::determinant(S, 3)) < 1e-9, "a singular matrix has determinant 0");
    }

    // --- 5. Determinant values. ---
    {
        CHECK(std::fabs(maz::math::determinant({3, 8, 4, 6}, 2) - (3.0 * 6.0 - 8.0 * 4.0)) < 1e-9,
              "2x2 determinant equals ad-bc");
        CHECK(std::fabs(maz::math::determinant({2, 0, 0, 0, 5, 0, 0, 0, 7}, 3) - 70.0) < 1e-9,
              "diagonal determinant is the product of the diagonal");
    }

    // --- 6. Inverse: A * inv(A) == I. ---
    {
        bool ok = true;
        for (int t = 0; t < 50; ++t) {
            const int n = 2 + static_cast<int>(rng.next() % 5u);
            std::vector<double> A(static_cast<std::size_t>(n * n));
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) A[static_cast<std::size_t>(i * n + j)] = rng.range(-2, 2);
                A[static_cast<std::size_t>(i * n + i)] += static_cast<double>(n) + 3.0;
            }
            std::vector<double> inv;
            if (!maz::math::invertMatrix(A, n, inv)) { ok = false; continue; }
            for (int i = 0; i < n && ok; ++i)
                for (int j = 0; j < n; ++j) {
                    double s = 0.0;
                    for (int k = 0; k < n; ++k) s += A[static_cast<std::size_t>(i * n + k)] * inv[static_cast<std::size_t>(k * n + j)];
                    const double expect = (i == j) ? 1.0 : 0.0;
                    if (std::fabs(s - expect) > 1e-6) ok = false;
                }
        }
        CHECK(ok, "A * inverse(A) equals the identity");
    }

    // --- 7. Determinism. ---
    {
        std::vector<double> A{4, 1, 2, 1, 5, 3, 2, 3, 6};
        std::vector<double> b{1, 2, 3};
        std::vector<double> x1, x2;
        maz::math::solveLinearSystem(A, b, 3, x1);
        maz::math::solveLinearSystem(A, b, 3, x2);
        CHECK(x1 == x2, "identical inputs produce identical solutions");
    }

    if (g_fail == 0) {
        std::printf("linearsolve: OK — known, round-trip, identity, singular, determinant, inverse, determinism.\n");
        return 0;
    }
    std::printf("linearsolve: %d failure(s).\n", g_fail);
    return 1;
}
