// tests/math/rayellipsoid.cpp — verifies ray vs axis-aligned ellipsoid (math RayEllipsoid.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rays, no <random>, no clock):
//   * SURFACE + NORMAL: every hit point satisfies the implicit equation sum(((p-c)/radii)^2)=1; the returned
//     normal is unit, points outward, and is PARALLEL to the analytic gradient (p-c)/radii^2 (the check that
//     catches the classic "warped direction" normal bug on non-uniform radii);
//   * BRUTE-FORCE ORACLE: for random rays, the analytic hit/miss and distance agree with a fine ray-march;
//   * SPHERE REDUCTION: with equal radii the result matches the closed-form ray-sphere distance;
//   * ANALYTIC: a ray fired down an axis at an axis-aligned ellipsoid hits the near cap at the exact t;
//   * determinism.
#include "maz/math/RayEllipsoid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

// The ellipsoid used by the random-ray test.
static const vec3 kC(1.0f, -2.0f, 0.5f);
static const vec3 kR(1.3f, 0.7f, 2.1f);

static float implicit(const vec3& p) {
    const vec3 q((p.x - kC.x) / kR.x, (p.y - kC.y) / kR.y, (p.z - kC.z) / kR.z);
    return dot(q, q);
}
static bool inside(const vec3& p) { return implicit(p) <= 1.0f; }

// Brute-force first-entry distance along the ray (or -1 if it never enters within range).
static float marchEntry(const vec3& from, const vec3& dir) {
    const float step = 0.001f;
    for (float t = 0.0f; t < 30.0f; t += step) {
        if (inside(from + dir * t)) return t;
    }
    return -1.0f;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 12.0f - 6.0f; }
};

int main() {
    // --- 1. Surface + normal + brute-force oracle over random rays. ---
    {
        Lcg rng{0xE11175u};
        int checked = 0, disagree = 0, hits = 0;
        float worstT = 0.0f, worstSurf = 0.0f, worstNormalCos = 1.0f;
        for (int i = 0; i < 8000; ++i) {
            const vec3 from(kC.x + rng.sym(), kC.y + rng.sym(), kC.z + rng.sym());
            if (inside(from)) continue; // "first entry" only well-defined outside
            // Aim at a random point near the ellipsoid surface so many rays hit; the spread also yields misses.
            const vec3 sdir(rng.sym(), rng.sym(), rng.sym());
            const float sl = len(sdir);
            if (sl < 1e-3f) continue;
            const vec3 surf(kC.x + kR.x * sdir.x / sl, kC.y + kR.y * sdir.y / sl, kC.z + kR.z * sdir.z / sl);
            const vec3 target = surf + vec3(rng.sym(), rng.sym(), rng.sym()) * 0.35f;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsEllipsoid(from, dir, kC, kR);
            const float bt = marchEntry(from, dir);
            ++checked;
            const bool bruteHit = bt >= 0.0f;
            if (hit.hit != bruteHit) {
                if (!hit.hit && bt > 0.02f) ++disagree;
                else if (hit.hit && bt < 0.0f && hit.t > 0.02f) ++disagree;
                continue;
            }
            if (hit.hit) {
                ++hits;
                worstT = std::max(worstT, std::fabs(hit.t - bt));
                worstSurf = std::max(worstSurf, std::fabs(implicit(hit.point) - 1.0f));
                CHECK(std::fabs(len(hit.normal) - 1.0f) < 1e-3f, "the hit normal is unit length");
                // Normal must be parallel to the analytic gradient (p-c)/radii^2.
                const vec3 rel = hit.point - kC;
                vec3 grad(rel.x / (kR.x * kR.x), rel.y / (kR.y * kR.y), rel.z / (kR.z * kR.z));
                grad = grad * (1.0f / len(grad));
                worstNormalCos = std::min(worstNormalCos, dot(hit.normal, grad));
            }
        }
        CHECK(checked > 3000 && hits > 300, "the random ray set exercises many hits");
        CHECK(disagree < 6, "analytic hit/miss agrees with the brute-force ray-march");
        CHECK(worstT < 0.01f, "analytic hit distance matches the brute-force entry distance");
        CHECK(worstSurf < 5e-3f, "every hit point lies on the ellipsoid surface");
        CHECK(worstNormalCos > 0.9999f, "the normal matches the analytic implicit gradient");
    }

    // --- 2. Sphere reduction: equal radii must match the closed-form ray-sphere hit. ---
    {
        const vec3 c(0.5f, 0.5f, 0.5f);
        const float R = 1.7f;
        Lcg rng{0x5EED01u};
        float worst = 0.0f;
        int compared = 0;
        for (int i = 0; i < 2000; ++i) {
            const vec3 from(c.x + rng.sym(), c.y + rng.sym(), c.z + rng.sym());
            // Aim near the sphere (surface point + jitter) so many rays actually hit; jitter still yields misses.
            vec3 sd(rng.sym(), rng.sym(), rng.sym());
            const float sl = len(sd);
            if (sl < 1e-3f) continue;
            const vec3 target = c + sd * (R / sl) + vec3(rng.sym(), rng.sym(), rng.sym()) * 0.4f;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsEllipsoid(from, dir, c, vec3(R, R, R));
            // Closed-form ray-sphere (nearest forward root).
            const vec3 oc = from - c;
            const float b = 2.0f * dot(oc, dir), cc = dot(oc, oc) - R * R;
            const float disc = b * b - 4.0f * cc;
            bool sHit = false; float sT = 0.0f;
            if (disc >= 0.0f) {
                const float s = std::sqrt(disc);
                float t = (-b - s) * 0.5f;
                if (t < 0.0f) t = (-b + s) * 0.5f;
                if (t >= 0.0f) { sHit = true; sT = t; }
            }
            CHECK(hit.hit == sHit, "ellipsoid with equal radii agrees with ray-sphere on hit/miss");
            if (hit.hit && sHit) { worst = std::max(worst, std::fabs(hit.t - sT)); ++compared; }
        }
        CHECK(compared > 200 && worst < 1e-3f, "equal-radii ellipsoid distance matches ray-sphere");
    }

    // --- 3. Analytic: ray down the x-axis at an axis-aligned ellipsoid hits the near cap. ---
    {
        const vec3 c(0, 0, 0), r(2.0f, 3.0f, 4.0f);
        const auto hit = maz::math::rayIntersectsEllipsoid(vec3(10, 0, 0), vec3(-1, 0, 0), c, r);
        CHECK(hit.hit && std::fabs(hit.t - 8.0f) < 1e-4f, "ray hits the +x cap at x = radii.x");
        CHECK(len(hit.point - vec3(2, 0, 0)) < 1e-4f, "the cap hit point is at (radii.x,0,0)");
        CHECK(len(hit.normal - vec3(1, 0, 0)) < 1e-4f, "the +x cap normal is +x");
    }

    // --- 4. Miss + determinism. ---
    {
        const vec3 c(0, 0, 0), r(1.0f, 1.0f, 1.0f);
        const auto miss = maz::math::rayIntersectsEllipsoid(vec3(5, 5, 0), vec3(0, 0, 1), c, r);
        CHECK(!miss.hit, "a ray that never approaches the ellipsoid misses");
        const auto a = maz::math::rayIntersectsEllipsoid(vec3(10, 0, 0), vec3(-1, 0, 0), c, vec3(2, 3, 4));
        const auto b = maz::math::rayIntersectsEllipsoid(vec3(10, 0, 0), vec3(-1, 0, 0), c, vec3(2, 3, 4));
        CHECK(a.hit == b.hit && a.t == b.t, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("rayellipsoid: OK — surface/normal, brute-force oracle, sphere reduction, analytic, "
                    "determinism.\n");
        return 0;
    }
    std::printf("rayellipsoid: %d failure(s).\n", g_fail);
    return 1;
}
