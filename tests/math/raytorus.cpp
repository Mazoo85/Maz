// tests/math/raytorus.cpp — verifies the quartic solver (Polynomial.hpp) and ray vs torus (RayTorus.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rays, no <random>, no clock):
//   * QUARTIC SOLVER: recovers the exact roots of factored quartics (distinct, mixed real/complex, and a
//     double root) and every returned root evaluates the polynomial to ~0;
//   * TORUS SURFACE + NORMAL: every hit lies on the torus (distance from the tube centre circle == minor
//     radius r), the normal is unit and points outward along that radial;
//   * BRUTE-FORCE ORACLE: for random rays the analytic hit/miss and distance agree with a fine ray-march;
//   * ANALYTIC: an axis-aligned torus hit at a hand-computed distance; a ray through the central hole misses;
//   * determinism.
#include "maz/math/RayTorus.hpp"
#include "maz/math/Polynomial.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

// Torus used by the random-ray test: centre, unit axis, major R, minor r.
static const vec3 kC(0.5f, -1.0f, 0.4f);
static vec3 kAxis() { const vec3 a(0.3f, 0.8f, 0.5f); return a * (1.0f / len(a)); }
static const float kR = 2.0f;
static const float kr = 0.6f;

// Distance from a world point to the torus surface's implicit "on-surface" measure: |dist-to-centre-circle - r|.
static float distToTube(const vec3& p) {
    const vec3 w = kAxis();
    const vec3 rel = p - kC;
    const float axialz = dot(rel, w);
    const vec3 planar = rel - w * axialz;
    const float rho = len(planar);           // distance from axis
    const float dCircle = std::sqrt((rho - kR) * (rho - kR) + axialz * axialz);
    return dCircle; // == r on the surface, < r inside
}
static bool insideTorus(const vec3& p) { return distToTube(p) <= kr; }

static float marchEntry(const vec3& from, const vec3& dir) {
    const float step = 0.0015f;
    for (float t = 0.0f; t < 30.0f; t += step) {
        if (insideTorus(from + dir * t)) return t;
    }
    return -1.0f;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 8.0f - 4.0f; }
};

int main() {
    // --- 0. Quartic solver on known factored polynomials. ---
    {
        // (x-1)(x-2)(x-3)(x-4) = x^4 -10x^3 +35x^2 -50x +24.
        auto r1 = maz::math::solveQuartic(1, -10, 35, -50, 24);
        CHECK(r1.count == 4, "quartic finds 4 distinct real roots");
        const float want1[4] = {1, 2, 3, 4};
        for (int i = 0; i < 4 && i < r1.count; ++i) CHECK(std::fabs(r1.x[i] - want1[i]) < 1e-3f, "roots 1..4");
        // (x^2+1)(x-5)(x+3) = x^4 -2x^3 -14x^2 -2x -15 : real roots -3, 5.
        auto r2 = maz::math::solveQuartic(1, -2, -14, -2, -15);
        CHECK(r2.count == 2 && std::fabs(r2.x[0] + 3.0f) < 1e-3f && std::fabs(r2.x[1] - 5.0f) < 1e-3f,
              "quartic finds the 2 real roots, ignoring the complex pair");
        // (x-2)^2 (x^2+1) = x^4 -4x^3 +5x^2 -4x +4 : double real root at 2.
        auto r3 = maz::math::solveQuartic(1, -4, 5, -4, 4);
        CHECK(r3.count >= 1 && std::fabs(r3.x[0] - 2.0f) < 2e-3f, "quartic recovers a double root at 2");
        // Every returned root evaluates the polynomial to ~0.
        auto evalq = [](const float k[5], float x) { return (((k[0] * x + k[1]) * x + k[2]) * x + k[3]) * x + k[4]; };
        const float poly[5] = {1, -10, 35, -50, 24};
        for (int i = 0; i < r1.count; ++i) CHECK(std::fabs(evalq(poly, r1.x[i])) < 1e-2f, "root evaluates to ~0");
    }

    const vec3 ax = kAxis();

    // --- 1. Torus surface + normal + brute-force oracle over random rays. ---
    {
        Lcg rng{0x7025u};
        const vec3 tmp = std::fabs(ax.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 1, 0);
        vec3 U = tmp - ax * dot(tmp, ax);
        U = U * (1.0f / len(U));
        const vec3 V = vec3(ax.y * U.z - ax.z * U.y, ax.z * U.x - ax.x * U.z, ax.x * U.y - ax.y * U.x);
        int checked = 0, disagree = 0, hits = 0;
        float worstT = 0.0f, worstSurf = 0.0f, worstNorm = 1.0f;
        const float pi = 3.14159265358979324f;
        for (int i = 0; i < 12000; ++i) {
            const vec3 from(kC.x + rng.sym(), kC.y + rng.sym(), kC.z + rng.sym());
            if (insideTorus(from)) continue;
            // Aim at a random point on the tube surface so many rays hit.
            const float phi = (rng.sym() + 4.0f) / 8.0f * 2.0f * pi; // around the ring
            const float th = (rng.sym() + 4.0f) / 8.0f * 2.0f * pi;  // around the tube
            const vec3 ringDir = U * std::cos(phi) + V * std::sin(phi);
            const vec3 surf = kC + ringDir * (kR + kr * std::cos(th)) + ax * (kr * std::sin(th));
            const vec3 target = surf + vec3(rng.sym(), rng.sym(), rng.sym()) * 0.3f;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsTorus(from, dir, kC, ax, kR, kr);
            const float bt = marchEntry(from, dir);
            ++checked;
            const bool bruteHit = bt >= 0.0f;
            if (hit.hit != bruteHit) {
                if (!hit.hit && bt > 0.03f) ++disagree;
                else if (hit.hit && bt < 0.0f && hit.t > 0.03f) ++disagree;
                continue;
            }
            if (hit.hit) {
                ++hits;
                worstT = std::max(worstT, std::fabs(hit.t - bt));
                worstSurf = std::max(worstSurf, std::fabs(distToTube(hit.point) - kr));
                worstNorm = std::min(worstNorm, len(hit.normal));
            }
        }
        CHECK(checked > 4000 && hits > 300, "the random ray set exercises many torus hits");
        CHECK(disagree < 12, "analytic hit/miss agrees with the brute-force ray-march");
        CHECK(worstT < 0.012f, "analytic hit distance matches the brute-force entry distance");
        CHECK(worstSurf < 6e-3f, "every hit point lies on the torus surface (distance to tube circle == r)");
        CHECK(std::fabs(worstNorm - 1.0f) < 1e-3f, "hit normals are unit length");
    }

    // --- 2. Analytic: axis-aligned torus (axis +z), ray toward -x hits the outer rim at x = R + r. ---
    {
        const vec3 c(0, 0, 0), axis(0, 0, 1);
        const float R = 3.0f, r = 0.5f;
        const auto hit = maz::math::rayIntersectsTorus(vec3(10, 0, 0), vec3(-1, 0, 0), c, axis, R, r);
        CHECK(hit.hit && std::fabs(hit.t - (10.0f - (R + r))) < 1e-3f, "ray hits the outer rim at x = R + r");
        CHECK(len(hit.point - vec3(R + r, 0, 0)) < 2e-3f, "the outer-rim hit point is at (R+r,0,0)");
        CHECK(len(hit.normal - vec3(1, 0, 0)) < 3e-3f, "the outer-rim normal points +x");
    }

    // --- 3. Miss: a ray straight through the central hole (along the axis) misses. ---
    {
        const vec3 c(0, 0, 0), axis(0, 0, 1);
        const auto hit = maz::math::rayIntersectsTorus(vec3(0, 0, 10), vec3(0, 0, -1), c, axis, 3.0f, 0.5f);
        CHECK(!hit.hit, "a ray down the axis through the hole misses the torus");
    }

    // --- 4. Determinism. ---
    {
        const auto a = maz::math::rayIntersectsTorus(vec3(10, 0, 0), vec3(-1, 0, 0), vec3(0, 0, 0), vec3(0, 0, 1), 3.0f, 0.5f);
        const auto b = maz::math::rayIntersectsTorus(vec3(10, 0, 0), vec3(-1, 0, 0), vec3(0, 0, 0), vec3(0, 0, 1), 3.0f, 0.5f);
        CHECK(a.hit == b.hit && a.t == b.t, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("raytorus: OK — quartic solver, torus surface/normal, brute-force oracle, analytic, "
                    "determinism.\n");
        return 0;
    }
    std::printf("raytorus: %d failure(s).\n", g_fail);
    return 1;
}
