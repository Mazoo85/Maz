// tests/math/raycylinder.cpp — verifies ray vs finite capped cylinder (math RayCylinder.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rays, no <random>, no clock):
//   * SURFACE + NORMAL: a side hit lies at exactly `radius` from the axis and within the length; a cap hit
//     lies within `radius` of a cap plane; the returned normal is unit and points outward;
//   * BRUTE-FORCE ORACLE: for random rays, whether-and-where the analytic query hits agrees with a fine
//     ray-march (step until the first sample is inside the cylinder) in both hit/miss and distance;
//   * ANALYTIC: a ray fired straight at an axis-aligned cylinder hits the near side at the expected t; a ray
//     down the axis hits the end cap; a ray passing outside the radius misses;
//   * determinism.
#include "maz/math/RayCylinder.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

// The finite cylinder used by the tests: base, unit axis, radius, height.
static const vec3 kBase(1.0f, -2.0f, 0.5f);
static vec3 kAxis() { const vec3 a(0.3f, 0.9f, -0.2f); return a * (1.0f / len(a)); }
static const float kR = 1.3f;
static const float kH = 4.0f;

static bool insideCyl(const vec3& p) {
    const vec3 ax = kAxis();
    const float h = dot(p - kBase, ax);
    if (h < 0.0f || h > kH) return false;
    const vec3 radial = (p - kBase) - ax * h;
    return dot(radial, radial) <= kR * kR;
}

// Brute-force first-entry distance along the ray (or -1 if it never enters within range).
static float marchEntry(const vec3& from, const vec3& dir) {
    const float step = 0.002f;
    for (float t = 0.0f; t < 30.0f; t += step) {
        if (insideCyl(from + dir * t)) return t;
    }
    return -1.0f;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 10.0f - 5.0f; }
};

int main() {
    const vec3 ax = kAxis();

    // --- 1. Surface + normal properties + brute-force oracle over random rays. ---
    {
        Lcg rng{0xC17Du};
        // Perpendicular basis for aiming rays at the cylinder.
        const vec3 tmp = std::fabs(ax.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
        vec3 u = tmp - ax * dot(tmp, ax);
        u = u * (1.0f / len(u));
        const vec3 w = vec3(ax.y * u.z - ax.z * u.y, ax.z * u.x - ax.x * u.z, ax.x * u.y - ax.y * u.x);
        int checked = 0, disagree = 0, hits = 0;
        float worstT = 0.0f, worstSurf = 0.0f;
        for (int i = 0; i < 6000; ++i) {
            const vec3 from(kBase.x + rng.sym(), kBase.y + rng.sym(), kBase.z + rng.sym());
            if (insideCyl(from)) continue; // "first entry" is only well-defined for rays starting outside
            // Aim at a random point on/near the cylinder so many rays actually hit; a spread past the
            // radius/ends still generates plenty of misses for the hit/miss oracle.
            const float hh = (rng.sym() * 0.5f + 0.5f) * kH * 1.3f - kH * 0.15f; // ~[-0.15H, 1.15H]
            const float rr = (rng.sym() * 0.5f + 0.5f) * kR * 1.4f;              // ~[0, 1.4R]
            const float ang = rng.sym() * 0.628318f;
            const vec3 target = kBase + ax * hh + (u * std::cos(ang) + w * std::sin(ang)) * rr;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsCylinder(from, dir, kBase, ax, kR, kH);
            const float bt = marchEntry(from, dir);
            ++checked;
            const bool bruteHit = bt >= 0.0f;
            // Skip a thin ambiguous band near grazing where the march step and analytic edge disagree.
            if (hit.hit != bruteHit) {
                // Only count as a real disagreement if the brute entry is clearly not near our t.
                if (!hit.hit && bt > 0.02f) ++disagree;
                else if (hit.hit && bt < 0.0f && hit.t > 0.02f) ++disagree;
                continue;
            }
            if (hit.hit) {
                ++hits;
                worstT = std::max(worstT, std::fabs(hit.t - bt));
                // Normal is unit.
                CHECK(std::fabs(len(hit.normal) - 1.0f) < 1e-3f, "the hit normal is unit length");
                // Point is on the surface: either radial==R (side) or on a cap plane within R.
                const float h = dot(hit.point - kBase, ax);
                const vec3 radial = (hit.point - kBase) - ax * h;
                const float rl = len(radial);
                const bool onSide = std::fabs(rl - kR) < 1e-2f && h > -1e-2f && h < kH + 1e-2f;
                const bool onCap = (std::fabs(h) < 1e-2f || std::fabs(h - kH) < 1e-2f) && rl <= kR + 1e-2f;
                if (!onSide && !onCap) worstSurf = std::max(worstSurf, 1.0f);
            }
        }
        CHECK(checked > 3000 && hits > 300, "the random ray set exercises many hits");
        CHECK(disagree < 5, "analytic hit/miss agrees with the brute-force ray-march");
        CHECK(worstT < 0.01f, "analytic hit distance matches the brute-force entry distance");
        CHECK(worstSurf == 0.0f, "every hit point lies on the cylinder's side or a cap");
    }

    // --- 2. Analytic: axis-aligned cylinder, ray straight at the near side. ---
    {
        const vec3 base(0, 0, 0), axis(0, 1, 0);
        // Ray from (5,1,0) toward -x; cylinder radius 2 at origin, height 4 (y in [0,4]).
        const auto hit = maz::math::rayIntersectsCylinder(vec3(5, 1, 0), vec3(-1, 0, 0), base, axis, 2.0f, 4.0f);
        CHECK(hit.hit && std::fabs(hit.t - 3.0f) < 1e-4f, "ray hits the near side at t = distance - radius");
        CHECK(len(hit.point - vec3(2, 1, 0)) < 1e-4f, "the side hit point is at (radius, y, 0)");
        CHECK(len(hit.normal - vec3(1, 0, 0)) < 1e-4f, "the side normal points radially outward");
    }

    // --- 3. Cap hit: ray down the axis hits the end cap. ---
    {
        const vec3 base(0, 0, 0), axis(0, 1, 0);
        const auto hit = maz::math::rayIntersectsCylinder(vec3(0.5f, 10.0f, 0.0f), vec3(0, -1, 0), base, axis, 2.0f, 4.0f);
        CHECK(hit.hit && std::fabs(hit.t - 6.0f) < 1e-4f, "ray down the axis hits the top cap at y=4");
        CHECK(len(hit.normal - vec3(0, 1, 0)) < 1e-4f, "the top-cap normal is +axis");
    }

    // --- 4. Miss: ray passing outside the radius. ---
    {
        const vec3 base(0, 0, 0), axis(0, 1, 0);
        const auto hit = maz::math::rayIntersectsCylinder(vec3(5, 1, 0), vec3(0, 0, 1), base, axis, 2.0f, 4.0f);
        CHECK(!hit.hit, "a ray that never approaches within the radius misses");
    }

    // --- 5. Determinism. ---
    {
        const auto a = maz::math::rayIntersectsCylinder(vec3(5, 1, 0), vec3(-1, 0, 0), vec3(0, 0, 0), vec3(0, 1, 0), 2.0f, 4.0f);
        const auto b = maz::math::rayIntersectsCylinder(vec3(5, 1, 0), vec3(-1, 0, 0), vec3(0, 0, 0), vec3(0, 1, 0), 2.0f, 4.0f);
        CHECK(a.hit == b.hit && a.t == b.t, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("raycylinder: OK — surface/normal, brute-force oracle, analytic side/cap, miss, "
                    "determinism.\n");
        return 0;
    }
    std::printf("raycylinder: %d failure(s).\n", g_fail);
    return 1;
}
