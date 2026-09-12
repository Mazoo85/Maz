// tests/math/kabsch.cpp — verifies best-fit rigid transform (math Kabsch.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * given a point cloud transformed by a KNOWN random rotation + translation, kabsch recovers a transform
//     that maps the originals onto the targets to within tight tolerance (thousands of random cases);
//   * it stays accurate under small per-point noise (recovered fit error near the noise floor, not worse);
//   * identity in -> identity-ish out; a pure translation is recovered with ~no rotation;
//   * a length mismatch is reported via ok=false.
#include "maz/math/Kabsch.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::kabsch;
using maz::math::quat;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float sym() { return (static_cast<float>(next()) / 4294967296.0f) * 2.0f - 1.0f; } // [-1,1)
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

static quat randomQuat(Lcg& rng) {
    const float u1 = rng.unit(), u2 = rng.unit(), u3 = rng.unit();
    const float s1 = std::sqrt(1.0f - u1), s2 = std::sqrt(u1);
    const float twoPi = 6.2831853f;
    quat q;
    q.x = s1 * std::sin(twoPi * u2);
    q.y = s1 * std::cos(twoPi * u2);
    q.z = s2 * std::sin(twoPi * u3);
    q.w = s2 * std::cos(twoPi * u3);
    return q;
}

// Max residual: how far the fitted transform leaves each mapped point from its target.
static float maxResidual(const maz::math::RigidTransform& T,
                         const std::vector<vec3>& from, const std::vector<vec3>& to) {
    float worst = 0.0f;
    for (std::size_t i = 0; i < from.size(); ++i) {
        const vec3 d = T.apply(from[i]) - to[i];
        const float e = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (e > worst) worst = e;
    }
    return worst;
}

int main() {
    // --- 1. Recover a known transform exactly (no noise). ---
    {
        Lcg r{0xCA8501u};
        bool ok = true;
        for (int trial = 0; trial < 3000 && ok; ++trial) {
            const int n = 3 + static_cast<int>(r.next() % 20u); // >=3 points for a well-posed 3D fit
            std::vector<vec3> from, to;
            const quat R = randomQuat(r);
            const vec3 t{r.sym() * 10.0f, r.sym() * 10.0f, r.sym() * 10.0f};
            for (int i = 0; i < n; ++i) {
                const vec3 p{r.sym() * 5.0f, r.sym() * 5.0f, r.sym() * 5.0f};
                from.push_back(p);
                to.push_back(R * p + t);
            }
            const auto T = kabsch(from, to);
            if (maxResidual(T, from, to) > 1e-3f) { ok = false; break; }
        }
        CHECK(ok, "recovers a known rotation+translation to within 1e-3");
    }

    // --- 2. Robust under small noise (fit error near the noise floor). ---
    {
        Lcg r{0x9015Eu};
        bool ok = true;
        for (int trial = 0; trial < 1000 && ok; ++trial) {
            const int n = 20;
            std::vector<vec3> from, to;
            const quat R = randomQuat(r);
            const vec3 t{r.sym() * 4.0f, r.sym() * 4.0f, r.sym() * 4.0f};
            const float noise = 0.02f;
            for (int i = 0; i < n; ++i) {
                const vec3 p{r.sym() * 5.0f, r.sym() * 5.0f, r.sym() * 5.0f};
                from.push_back(p);
                vec3 q = R * p + t;
                q.x += r.sym() * noise; q.y += r.sym() * noise; q.z += r.sym() * noise;
                to.push_back(q);
            }
            const auto T = kabsch(from, to);
            // With 3*noise slack the least-squares fit should sit comfortably below this bound.
            if (maxResidual(T, from, to) > 0.15f) { ok = false; break; }
        }
        CHECK(ok, "stays accurate under small per-point noise");
    }

    // --- 3. Identity and pure translation. ---
    {
        std::vector<vec3> pts{{1, 0, 0}, {0, 2, 0}, {0, 0, 3}, {1, 1, 1}};
        const auto I = kabsch(pts, pts);
        CHECK(maxResidual(I, pts, pts) < 1e-4f, "identity fit maps points onto themselves");
        // Rotation should be ~identity (|w| ~ 1).
        CHECK(std::fabs(std::fabs(I.rotation.w) - 1.0f) < 1e-3f, "identity fit rotation is ~identity");

        std::vector<vec3> shifted;
        const vec3 t{3.0f, -2.0f, 5.0f};
        for (const vec3& p : pts) shifted.push_back(p + t);
        const auto T = kabsch(pts, shifted);
        CHECK(maxResidual(T, pts, shifted) < 1e-4f, "pure translation recovered");
        CHECK(std::fabs(std::fabs(T.rotation.w) - 1.0f) < 1e-3f, "pure translation has ~no rotation");
    }

    // --- 4. Length mismatch reported. ---
    {
        std::vector<vec3> a{{0, 0, 0}, {1, 0, 0}};
        std::vector<vec3> b{{0, 0, 0}};
        bool ok = true;
        kabsch(a, b, &ok);
        CHECK(!ok, "length mismatch sets ok=false");
    }

    if (g_fail == 0) {
        std::printf("kabsch: OK — exact recovery, noise robustness, identity/translation, mismatch guard.\n");
        return 0;
    }
    std::printf("kabsch: %d failure(s).\n", g_fail);
    return 1;
}
