// tests/math/planefit.cpp — verifies total-least-squares plane fitting (math ShapeFit.hpp fitPlane).
// Ground truths, deterministic (seeded LCG, no <random>, no clock):
//   * EXACT: points lying exactly on a known plane recover that plane's normal (up to sign) and satisfy
//     normal.p + d ~ 0;
//   * NOISY: with small out-of-plane jitter the normal stays aligned and the orthogonal residual is small;
//   * MIN-VARIANCE: the fitted normal is the direction of least spread (variance along it is far smaller
//     than along an in-plane direction) — the defining total-least-squares property;
//   * the plane passes through the centroid; too few points fail;
//   * determinism.
#include "maz/math/ShapeFit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

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

static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static vec3 norm3(const vec3& a) { const float l = std::sqrt(dot3(a, a)); return vec3(a.x / l, a.y / l, a.z / l); }

int main() {
    Lcg rng{0x71A3u};

    // Ground-truth plane: normal n0 (unit), passing through p0. Build two in-plane basis vectors.
    const vec3 n0 = norm3(vec3(0.3f, 0.8f, -0.5f));
    const vec3 p0(2.0f, -1.0f, 4.0f);
    vec3 u = norm3(vec3(-n0.y, n0.x, 0.0f)); // perpendicular to n0
    vec3 w = vec3(n0.y * u.z - n0.z * u.y, n0.z * u.x - n0.x * u.z, n0.x * u.y - n0.y * u.x); // n0 x u

    // --- 1. Exact plane. ---
    {
        std::vector<vec3> pts;
        for (int i = 0; i < 40; ++i) {
            const float s = rng.range(-5, 5), t = rng.range(-5, 5);
            pts.push_back(vec3(p0.x + s * u.x + t * w.x, p0.y + s * u.y + t * w.y, p0.z + s * u.z + t * w.z));
        }
        const auto f = maz::math::fitPlane(pts);
        CHECK(f.ok, "the plane fit succeeds");
        CHECK(std::fabs(std::fabs(dot3(f.normal, n0)) - 1.0f) < 1e-3f,
              "the recovered normal matches the true normal (up to sign)");
        float maxOff = 0.0f;
        for (const vec3& p : pts) maxOff = std::max(maxOff, std::fabs(dot3(f.normal, p) + f.d));
        CHECK(maxOff < 1e-3f, "every point satisfies normal.p + d ~ 0");
    }

    // --- 2. Noisy plane: aligned normal, small residual, min-variance. ---
    {
        std::vector<vec3> pts;
        for (int i = 0; i < 200; ++i) {
            const float s = rng.range(-5, 5), t = rng.range(-5, 5), off = rng.range(-0.05f, 0.05f);
            pts.push_back(vec3(p0.x + s * u.x + t * w.x + off * n0.x,
                               p0.y + s * u.y + t * w.y + off * n0.y,
                               p0.z + s * u.z + t * w.z + off * n0.z));
        }
        const auto f = maz::math::fitPlane(pts);
        CHECK(f.ok && std::fabs(std::fabs(dot3(f.normal, n0)) - 1.0f) < 1e-2f,
              "a noisy plane still recovers the true normal");
        // Variance along the normal is much smaller than along an in-plane direction.
        double varN = 0.0, varU = 0.0;
        for (const vec3& p : pts) {
            const vec3 dpv(p.x - f.centroid.x, p.y - f.centroid.y, p.z - f.centroid.z);
            const float an = dot3(dpv, f.normal), au = dot3(dpv, u);
            varN += static_cast<double>(an) * an;
            varU += static_cast<double>(au) * au;
        }
        CHECK(varN < varU * 0.05, "the normal is the direction of least spread (total least squares)");
    }

    // --- 3. Centroid membership + degenerate. ---
    {
        std::vector<vec3> pts{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0.5f, 0.5f, 0}};
        const auto f = maz::math::fitPlane(pts);
        CHECK(f.ok && std::fabs(dot3(f.normal, f.centroid) + f.d) < 1e-4f, "the plane passes through the centroid");
        CHECK(std::fabs(std::fabs(f.normal.z) - 1.0f) < 1e-3f, "a z=0 point set fits the z-plane (normal ~ Z)");
        std::vector<vec3> tooFew{{0, 0, 0}, {1, 1, 1}};
        CHECK(!maz::math::fitPlane(tooFew).ok, "fewer than three points cannot fit a plane");
    }

    // --- 4. Determinism. ---
    {
        std::vector<vec3> pts{{0, 0, 0}, {1, 0, 0.1f}, {0, 1, -0.1f}, {1, 1, 0.2f}, {2, 1, 0.0f}};
        const auto a = maz::math::fitPlane(pts);
        const auto b = maz::math::fitPlane(pts);
        CHECK(a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z && a.d == b.d,
              "identical inputs produce identical plane fits");
    }

    if (g_fail == 0) {
        std::printf("planefit: OK — exact, noisy, min-variance, centroid, degenerate, determinism.\n");
        return 0;
    }
    std::printf("planefit: %d failure(s).\n", g_fail);
    return 1;
}
