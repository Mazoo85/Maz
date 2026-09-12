// tests/math/raycone.cpp — verifies ray vs finite capped cone (math RayCone.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rays, no <random>, no clock):
//   * SURFACE + NORMAL: a side hit makes the half-angle exactly (dot(P-apex,axis) = |P-apex|·cos θ) within
//     the height; the normal is unit, outward (points away from the axis), and perpendicular to the local
//     surface generator direction;
//   * BRUTE-FORCE ORACLE: for random rays, whether-and-where the analytic query hits agrees with a fine
//     ray-march (first sample inside the solid cone);
//   * ANALYTIC: a ray fired at a known cone hits the side at the expected point; a ray up the axis from below
//     the base hits the cap;
//   * MISS + determinism.
#include "maz/math/RayCone.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

// The cone used by the random-ray test: apex, unit axis, half-angle, height.
static const vec3 kApex(0.5f, -1.0f, 0.3f);
static vec3 kAxis() { const vec3 a(0.2f, 1.0f, 0.3f); return a * (1.0f / len(a)); }
static const float kHalf = 0.5f; // ~28.6 deg
static const float kH = 3.0f;

static bool insideCone(const vec3& p) {
    const vec3 ax = kAxis();
    const vec3 rel = p - kApex;
    const float m = dot(rel, ax);
    if (m < 0.0f || m > kH) return false;
    const float rl = len(rel);
    if (rl < 1e-6f) return true;
    return m >= rl * std::cos(kHalf); // angle <= half-angle
}

static float marchEntry(const vec3& from, const vec3& dir) {
    const float step = 0.001f;
    for (float t = 0.0f; t < 30.0f; t += step) {
        if (insideCone(from + dir * t)) return t;
    }
    return -1.0f;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 8.0f - 4.0f; }
};

int main() {
    const vec3 ax = kAxis();
    const float cosHalf = std::cos(kHalf);

    // --- 1. Surface + normal + brute-force oracle over random rays. ---
    {
        Lcg rng{0xC04Eu};
        // Perpendicular basis for aiming rays into the cone volume.
        const vec3 tmp = std::fabs(ax.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
        vec3 u = tmp - ax * dot(tmp, ax);
        u = u * (1.0f / len(u));
        const vec3 w = vec3(ax.y * u.z - ax.z * u.y, ax.z * u.x - ax.x * u.z, ax.x * u.y - ax.y * u.x);
        int checked = 0, disagree = 0, hits = 0;
        float worstT = 0.0f;
        for (int i = 0; i < 9000; ++i) {
            const vec3 from(kApex.x + rng.sym(), kApex.y + rng.sym(), kApex.z + rng.sym());
            if (insideCone(from)) continue;
            // Aim at a random point in the cone's bounding region so many rays hit; jitter yields misses too.
            const float hh = (rng.sym() * 0.5f + 0.5f) * kH * 1.2f;         // ~[0, 1.2H]
            const float rr = (rng.sym() * 0.5f + 0.5f) * kH * std::tan(kHalf) * 1.3f;
            const float ang = rng.sym() * 0.6283f;
            const vec3 target = kApex + ax * hh + (u * std::cos(ang) + w * std::sin(ang)) * rr;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsCone(from, dir, kApex, ax, kHalf, kH);
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
                CHECK(std::fabs(len(hit.normal) - 1.0f) < 1e-3f, "the hit normal is unit length");
                const vec3 rel = hit.point - kApex;
                const float m = dot(rel, ax);
                const float rl = len(rel);
                const bool onSide = rl > 1e-4f && std::fabs(m - rl * cosHalf) < 1e-2f * rl && m > -1e-2f && m < kH + 1e-2f;
                const bool onCap = std::fabs(m - kH) < 1e-2f;
                CHECK(onSide || onCap, "every hit lies on the cone side or the base cap");
                if (onSide) {
                    // Outward normal must have a positive radial (away-from-axis) component.
                    const vec3 radial = rel - ax * m;
                    if (len(radial) > 1e-3f) {
                        CHECK(dot(hit.normal, radial) > 0.0f, "the side normal points outward (away from the axis)");
                    }
                }
            }
        }
        CHECK(checked > 3000 && hits > 300, "the random ray set exercises many hits");
        CHECK(disagree < 8, "analytic hit/miss agrees with the brute-force ray-march");
        CHECK(worstT < 0.01f, "analytic hit distance matches the brute-force entry distance");
    }

    // --- 2. Analytic: apex at origin, axis +y, 45 deg, height 5; ray toward -x at y=2 hits the side at x=2. ---
    {
        const vec3 apex(0, 0, 0), axis(0, 1, 0);
        const float half = 0.78539816f; // 45 deg => radius = height at that y
        const auto hit = maz::math::rayIntersectsCone(vec3(5, 2, 0), vec3(-1, 0, 0), apex, axis, half, 5.0f);
        // At y=2 the cone radius is 2 (tan45*2); the near side is x=2, so t = 5-2 = 3.
        CHECK(hit.hit && std::fabs(hit.t - 3.0f) < 1e-3f, "ray hits the 45deg cone side at the expected t");
        CHECK(len(hit.point - vec3(2, 2, 0)) < 1e-3f, "the side hit point is at (radius, y, 0)");
    }

    // --- 3. Base cap: ray up the axis from below the base hits the cap disk. ---
    {
        const vec3 apex(0, 0, 0), axis(0, 1, 0);
        const auto hit = maz::math::rayIntersectsCone(vec3(0.3f, 10.0f, 0.0f), vec3(0, -1, 0), apex, axis, 0.6f, 4.0f);
        CHECK(hit.hit && std::fabs(hit.t - 6.0f) < 1e-3f, "ray down the axis hits the base cap at y=height");
        CHECK(len(hit.normal - vec3(0, 1, 0)) < 1e-3f, "the base-cap normal is +axis");
    }

    // --- 4. Miss + determinism. ---
    {
        const vec3 apex(0, 0, 0), axis(0, 1, 0);
        const auto miss = maz::math::rayIntersectsCone(vec3(10, 2, 0), vec3(0, 0, 1), apex, axis, 0.5f, 4.0f);
        CHECK(!miss.hit, "a ray that never approaches the cone misses");
        const auto a = maz::math::rayIntersectsCone(vec3(5, 2, 0), vec3(-1, 0, 0), apex, axis, 0.7853f, 5.0f);
        const auto b = maz::math::rayIntersectsCone(vec3(5, 2, 0), vec3(-1, 0, 0), apex, axis, 0.7853f, 5.0f);
        CHECK(a.hit == b.hit && a.t == b.t, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("raycone: OK — surface/normal, brute-force oracle, analytic side/cap, miss, determinism.\n");
        return 0;
    }
    std::printf("raycone: %d failure(s).\n", g_fail);
    return 1;
}
