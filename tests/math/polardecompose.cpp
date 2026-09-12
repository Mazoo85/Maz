// tests/math/polardecompose.cpp — verifies 3x3 polar decomposition M = R*S (math PolarDecompose.hpp).
// Ground truths, deterministic (seeded LCG for matrices, no <random>, no clock):
//   * a pure rotation decomposes to itself with S == identity;
//   * the strong analytic oracle: build M = R_true * S_sym from a KNOWN rotation and a known symmetric
//     positive-definite matrix, decompose, and recover R_true and S_sym exactly;
//   * invariants over many random inputs: R is orthonormal (R^T R == I) with det +1, S is symmetric, and
//     R*S reconstructs M;
//   * extractRotation returns the R factor; a left-handed / singular matrix reports failure.
#include "maz/math/PolarDecompose.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::mat3;
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

static float maxDiff(const mat3& a, const mat3& b) {
    float d = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) d = std::fmax(d, std::fabs(a[c][r] - b[c][r]));
    return d;
}

// A rotation from Euler-ish angles (glm quat -> mat3), guaranteed proper.
static mat3 rotationFrom(float ax, float ay, float az) {
    const glm::quat q = glm::quat(vec3{ax, ay, az});
    return glm::mat3_cast(glm::normalize(q));
}

int main() {
    // --- 1. Pure rotation -> itself, S == I. ---
    {
        const mat3 R = rotationFrom(0.4f, -0.9f, 1.7f);
        mat3 r, s;
        const bool ok = maz::math::polarDecompose(R, r, s);
        CHECK(ok, "pure rotation decomposes");
        CHECK(maxDiff(r, R) < 1e-4f, "R equals the input rotation");
        CHECK(maxDiff(s, mat3(1.0f)) < 1e-4f, "S is the identity for a pure rotation");
    }

    // --- 2. Analytic oracle + invariants over random R*S. ---
    {
        Lcg rng{0xB0A11u};
        bool recoverOk = true, orthoOk = true, detOk = true, symOk = true, reconOk = true;
        int count = 0;
        for (int trial = 0; trial < 2000 && recoverOk && orthoOk && detOk && symOk && reconOk; ++trial) {
            const mat3 Rtrue = rotationFrom(rng.range(-3.1f, 3.1f), rng.range(-3.1f, 3.1f), rng.range(-3.1f, 3.1f));
            // Symmetric positive-definite S = D + a*a^T with positive diagonal (keeps M non-degenerate, det>0).
            mat3 Ssym(0.0f);
            const float d0 = rng.range(0.5f, 2.0f), d1 = rng.range(0.5f, 2.0f), d2 = rng.range(0.5f, 2.0f);
            Ssym[0][0] = d0; Ssym[1][1] = d1; Ssym[2][2] = d2;
            const float o01 = rng.range(-0.25f, 0.25f), o02 = rng.range(-0.25f, 0.25f), o12 = rng.range(-0.25f, 0.25f);
            Ssym[1][0] = Ssym[0][1] = o01;
            Ssym[2][0] = Ssym[0][2] = o02;
            Ssym[2][1] = Ssym[1][2] = o12;

            const mat3 M = Rtrue * Ssym;
            ++count;
            mat3 r, s;
            if (!maz::math::polarDecompose(M, r, s)) { recoverOk = false; break; }

            if (maxDiff(r, Rtrue) > 2e-3f) recoverOk = false;
            if (maxDiff(s, Ssym) > 2e-3f) recoverOk = false;

            // R orthonormal, det +1.
            if (maxDiff(glm::transpose(r) * r, mat3(1.0f)) > 1e-3f) orthoOk = false;
            if (std::fabs(glm::determinant(r) - 1.0f) > 1e-3f) detOk = false;
            // S symmetric.
            if (maxDiff(s, glm::transpose(s)) > 1e-4f) symOk = false;
            // Reconstruction.
            if (maxDiff(r * s, M) > 2e-3f) reconOk = false;
        }
        CHECK(count > 1000, "the random battery ran");
        CHECK(recoverOk, "recovers the known R and S from M = R*S (analytic oracle)");
        CHECK(orthoOk, "R is orthonormal (R^T R == I)");
        CHECK(detOk, "R is a proper rotation (det +1)");
        CHECK(symOk, "S is symmetric");
        CHECK(reconOk, "R*S reconstructs M");
    }

    // --- 3. extractRotation convenience + failure on singular input. ---
    {
        const mat3 R = rotationFrom(1.0f, 0.2f, -0.5f);
        mat3 scale(0.0f);
        scale[0][0] = 2.0f; scale[1][1] = 0.5f; scale[2][2] = 1.5f;
        const mat3 M = R * scale;
        CHECK(maxDiff(maz::math::extractRotation(M), R) < 2e-3f, "extractRotation returns the rotation factor");

        mat3 singular(0.0f); // all zeros -> det 0
        mat3 r, s;
        CHECK(!maz::math::polarDecompose(singular, r, s), "singular matrix reports failure");
    }

    if (g_fail == 0) {
        std::printf("polardecompose: OK — pure rotation, analytic R*S recovery, invariants, extract/singular.\n");
        return 0;
    }
    std::printf("polardecompose: %d failure(s).\n", g_fail);
    return 1;
}
