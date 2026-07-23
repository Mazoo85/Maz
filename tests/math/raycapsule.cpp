// tests/math/raycapsule.cpp — verifies ray vs capsule (math RayCapsule.hpp).
// Ground truths, deterministic (fixed cases + seeded-LCG rays, no <random>, no clock):
//   * ANALYTIC CASES: an axis-aligned capsule gives the closed-form side hit and cap hit (distance,
//     point, normal) exactly;
//   * SURFACE IDENTITY (airtight): every reported hit lies on the capsule surface —
//     distance(point, spine segment) == radius — and the normal equals normalize(point − closestOnSpine);
//   * BRUTE-FORCE ORACLE: for random rays aimed at the capsule, the analytic first-hit distance matches
//     an independent fine ray-march that brackets the first sign change of dist(spine) − radius, then
//     bisects; misses agree with the march;
//   * DEGENERATE spine (a==b) behaves as a sphere.
#include "maz/math/RayCapsule.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Closest point on segment [a,b] to p (independent of the header).
static vec3 closestOnSeg(const vec3& p, const vec3& a, const vec3& b) {
    const vec3 ab = b - a;
    const float d2 = dot3(ab, ab);
    if (d2 < 1e-20f) {
        return a;
    }
    float t = dot3(p - a, ab) / d2;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return a + ab * t;
}
static float distToSeg(const vec3& p, const vec3& a, const vec3& b) { return len(p - closestOnSeg(p, a, b)); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; } // [-1,1)
};

int main() {
    using maz::math::rayIntersectsCapsule;

    // --- 1. Axis-aligned capsule: analytic side + cap hits. Spine (0,0,0)->(0,2,0), radius 0.5. ---
    {
        const vec3 a(0, 0, 0), b(0, 2, 0);
        const float r = 0.5f;
        // Side hit: ray from (-5,1,0) toward +x hits the cylinder wall at x=-0.5, t=4.5, normal (-1,0,0).
        auto s = rayIntersectsCapsule(vec3(-5, 1, 0), vec3(1, 0, 0), a, b, r);
        CHECK(s.hit, "side ray hits");
        CHECK(std::fabs(s.t - 4.5f) < 1e-4f, "side hit distance is 4.5");
        CHECK(len(s.point - vec3(-0.5f, 1, 0)) < 1e-4f, "side hit point is (-0.5,1,0)");
        CHECK(len(s.normal - vec3(-1, 0, 0)) < 1e-4f, "side normal points back along -x");
        // Top-cap hit: ray from (0,5,0) toward -y hits the sphere at b at y=2.5, t=2.5, normal (0,1,0).
        auto c = rayIntersectsCapsule(vec3(0, 5, 0), vec3(0, -1, 0), a, b, r);
        CHECK(c.hit, "cap ray hits");
        CHECK(std::fabs(c.t - 2.5f) < 1e-4f, "cap hit distance is 2.5");
        CHECK(len(c.point - vec3(0, 2.5f, 0)) < 1e-4f, "cap hit point is the pole of the top hemisphere");
        CHECK(len(c.normal - vec3(0, 1, 0)) < 1e-4f, "cap normal points up");
        // A ray passing farther than the radius from the spine misses.
        auto miss = rayIntersectsCapsule(vec3(-5, 1, 0.9f), vec3(1, 0, 0), a, b, r);
        CHECK(!miss.hit, "a ray beyond the radius misses");
    }

    // --- 2. Surface identity + normal identity + brute-force oracle over random aimed rays. ---
    {
        const vec3 a(-0.4f, 0.1f, 0.7f), b(1.3f, 2.1f, -0.5f); // generic slanted spine
        const float r = 0.6f;
        Lcg rng{0xCA95u};
        float worstSurf = 0.0f, worstNrm = 0.0f, worstT = 0.0f;
        int hits = 0, matched = 0, agreeMiss = 0, disagree = 0;
        for (int i = 0; i < 4000; ++i) {
            // Origin on a shell around the capsule; direction aimed near a random point on the spine
            // (so many rays hit, some graze/miss).
            const vec3 origin(rng.sym() * 4.0f, rng.sym() * 4.0f, rng.sym() * 4.0f);
            const float seg = (rng.sym() * 0.5f + 0.5f);
            const vec3 target = a + (b - a) * seg + vec3(rng.sym(), rng.sym(), rng.sym()) * (r * 1.3f);
            vec3 dir = target - origin;
            const float dl = len(dir);
            if (dl < 1e-4f) {
                continue;
            }
            dir = dir * (1.0f / dl); // unit ray, so t is a true distance

            auto h = rayIntersectsCapsule(origin, dir, a, b, r);

            // Brute-force first-hit: march f(t)=dist(spine)-r; bracket the first negative crossing, bisect.
            const float tMax = 12.0f, step = 0.004f;
            bool bruteHit = false;
            float bruteT = 0.0f;
            float prevF = distToSeg(origin, a, b) - r;
            if (prevF <= 0.0f) {
                // Started inside — skip (both analytic and brute treat starts-inside specially).
                continue;
            }
            for (float t = step; t <= tMax; t += step) {
                const vec3 p = origin + dir * t;
                const float f = distToSeg(p, a, b) - r;
                if (f <= 0.0f) {
                    float lo = t - step, hi = t;
                    for (int k = 0; k < 40; ++k) {
                        const float mid = 0.5f * (lo + hi);
                        const vec3 pm = origin + dir * mid;
                        if (distToSeg(pm, a, b) - r <= 0.0f) {
                            hi = mid;
                        } else {
                            lo = mid;
                        }
                    }
                    bruteHit = true;
                    bruteT = 0.5f * (lo + hi);
                    break;
                }
                prevF = f;
            }

            if (h.hit) {
                ++hits;
                // Surface identity: the hit lies on the capsule surface.
                worstSurf = std::max(worstSurf, std::fabs(distToSeg(h.point, a, b) - r));
                // Normal identity: outward normal == normalize(point - closest-on-spine).
                const vec3 c = closestOnSeg(h.point, a, b);
                const vec3 want = (h.point - c) * (1.0f / len(h.point - c));
                worstNrm = std::max(worstNrm, len(h.normal - want));
                // Point reconstructs from t.
                worstSurf = std::max(worstSurf, len(h.point - (origin + dir * h.t)));
            }
            // Agreement with the brute-force oracle.
            if (h.hit && bruteHit) {
                worstT = std::max(worstT, std::fabs(h.t - bruteT));
                ++matched;
            } else if (!h.hit && !bruteHit) {
                ++agreeMiss;
            } else {
                ++disagree; // one says hit, the other miss (only grazing/tangent rays near the surface)
            }
        }
        CHECK(hits > 800, "the aimed rays exercise many real hits");
        CHECK(matched > 800, "analytic and brute-force agree on which rays hit");
        CHECK(agreeMiss > 100, "analytic and brute-force agree on many clean misses");
        CHECK(disagree < 60, "hit/miss disagreements are rare and confined to near-tangent grazes");
        CHECK(worstSurf < 2e-3f, "every hit lies on the capsule surface and reconstructs from t");
        CHECK(worstNrm < 3e-3f, "the normal equals normalize(point - closest point on the spine)");
        CHECK(worstT < 3e-3f, "analytic first-hit distance matches the brute-force ray-march");
        // Analytic hits should virtually never disagree with the march about hitting at all.
        CHECK(hits - matched < 20, "analytic hits are corroborated by the march");
    }

    // --- 3. Degenerate spine (a==b) is a sphere of the given radius. ---
    {
        const vec3 a(1, 1, 1);
        const float r = 0.75f;
        auto h = rayIntersectsCapsule(vec3(1, 1, -5), vec3(0, 0, 1), a, a, r);
        CHECK(h.hit, "degenerate capsule (sphere) is hit");
        CHECK(std::fabs(h.t - (6.0f - r)) < 1e-4f, "sphere front hit distance is |center-origin| - r");
        CHECK(len(h.point - vec3(1, 1, 1 - r)) < 1e-4f, "sphere hit is the near pole");
        CHECK(len(h.normal - vec3(0, 0, -1)) < 1e-4f, "sphere normal points back at the ray");
    }

    if (g_fail == 0) {
        std::printf("raycapsule: OK — analytic side/cap, surface & normal identity, brute-force oracle, sphere.\n");
        return 0;
    }
    std::printf("raycapsule: %d failure(s).\n", g_fail);
    return 1;
}
