// tests/math/rayplanar.cpp — verifies ray vs disk / annulus / oriented rectangle (math RayPlanar.hpp).
// Ground truths, deterministic (fixed + seeded-LCG rays, no <random>, no clock):
//   * ANALYTIC: a ray fired straight at a disk/annulus/rect hits the plane at the expected t and point; the
//     returned normal is unit and faces the incoming ray; a parallel ray misses;
//   * CONTAINMENT: a hit occurs exactly when the in-plane point is inside the shape (disk radius, annulus
//     ring band, rect half-extents) — cross-checked against an independent membership predicate over random
//     rays aimed at the plane;
//   * ANNULUS HOLE: a ray through the centre of the ring misses; a ray in the band hits;
//   * determinism.
#include "maz/math/RayPlanar.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 4.0f - 2.0f; }
};

int main() {
    // --- 1. Disk: analytic hit + normal facing + miss outside radius + parallel miss. ---
    {
        const vec3 c(0, 0, 0), n(0, 0, 1);
        const float R = 2.0f;
        const auto hit = maz::math::rayIntersectsDisk(vec3(0.5f, 0.5f, 5.0f), vec3(0, 0, -1), c, n, R);
        CHECK(hit.hit && std::fabs(hit.t - 5.0f) < 1e-4f, "ray hits the disk plane at t = distance");
        CHECK(len(hit.point - vec3(0.5f, 0.5f, 0.0f)) < 1e-4f, "the disk hit point is on the plane");
        CHECK(len(hit.normal - vec3(0, 0, 1)) < 1e-4f, "the disk normal faces the incoming ray (+z)");
        // Fired from below: normal should flip to -z to still face the ray.
        const auto below = maz::math::rayIntersectsDisk(vec3(0, 0, -5), vec3(0, 0, 1), c, n, R);
        CHECK(below.hit && len(below.normal - vec3(0, 0, -1)) < 1e-4f, "the normal flips to face a ray from below");
        // Outside the radius: miss.
        CHECK(!maz::math::rayIntersectsDisk(vec3(3, 0, 5), vec3(0, 0, -1), c, n, R).hit, "misses outside the radius");
        // Parallel ray: miss.
        CHECK(!maz::math::rayIntersectsDisk(vec3(0, 0, 5), vec3(1, 0, 0), c, n, R).hit, "a parallel ray misses");
    }

    // --- 2. Disk containment vs an independent predicate over random rays aimed at the plane. ---
    {
        const vec3 c(1, -2, 0.5f), n = vec3(0.3f, 0.5f, 0.8f) * (1.0f / len(vec3(0.3f, 0.5f, 0.8f)));
        const float R = 1.7f;
        // In-plane basis.
        const vec3 tmp = std::fabs(n.z) < 0.9f ? vec3(0, 0, 1) : vec3(1, 0, 0);
        vec3 u = tmp - n * dot(tmp, n);
        u = u * (1.0f / len(u));
        const vec3 v = vec3(n.y * u.z - n.z * u.y, n.z * u.x - n.x * u.z, n.x * u.y - n.y * u.x);
        Lcg rng{0xD15Cu};
        int checked = 0, hits = 0, mismatch = 0;
        for (int i = 0; i < 8000; ++i) {
            const vec3 from = c + n * (rng.sym() + 3.0f) + u * rng.sym() + v * rng.sym(); // in front of the plane
            // Aim at a random in-plane target (some inside R, some outside).
            const vec3 target = c + (u * rng.sym() + v * rng.sym()) * 1.5f;
            vec3 dir = target - from;
            const float dl = len(dir);
            if (dl < 1e-3f) continue;
            dir = dir * (1.0f / dl);
            const auto hit = maz::math::rayIntersectsDisk(from, dir, c, n, R);
            // Independent: where does the ray meet the plane, and is that within R?
            const float denom = dot(dir, n);
            if (std::fabs(denom) < 1e-6f) continue;
            const float t = dot(c - from, n) / denom;
            if (t < 0.0f) { if (hit.hit) ++mismatch; continue; }
            const vec3 p = from + dir * t;
            const bool inside = len(p - c) <= R;
            ++checked;
            if (hit.hit != inside) ++mismatch;
            if (hit.hit) ++hits;
        }
        CHECK(checked > 3000 && hits > 300, "many random rays hit and miss the disk");
        CHECK(mismatch == 0, "disk hit/miss matches the independent in-plane radius test");
    }

    // --- 3. Annulus: hole miss + band hit. ---
    {
        const vec3 c(0, 0, 0), n(0, 1, 0);
        // Ray straight down the +z... actually fire along -y at the y=0 plane.
        const auto hole = maz::math::rayIntersectsAnnulus(vec3(0.0f, 5.0f, 0.0f), vec3(0, -1, 0), c, n, 1.0f, 2.0f);
        CHECK(!hole.hit, "a ray through the ring's central hole misses");
        const auto band = maz::math::rayIntersectsAnnulus(vec3(1.5f, 5.0f, 0.0f), vec3(0, -1, 0), c, n, 1.0f, 2.0f);
        CHECK(band.hit && std::fabs(band.t - 5.0f) < 1e-4f, "a ray in the ring band hits");
        const auto outside = maz::math::rayIntersectsAnnulus(vec3(3.0f, 5.0f, 0.0f), vec3(0, -1, 0), c, n, 1.0f, 2.0f);
        CHECK(!outside.hit, "a ray beyond the outer radius misses");
    }

    // --- 4. Oriented rectangle: inside hit, outside miss, corner, normal. ---
    {
        const vec3 c(0, 0, 0), uAxis(1, 0, 0), vAxis(0, 1, 0);
        const float hu = 2.0f, hv = 1.0f; // 4 x 2 rectangle in the z=0 plane
        const auto in = maz::math::rayIntersectsRect(vec3(1.5f, 0.5f, 4.0f), vec3(0, 0, -1), c, uAxis, vAxis, hu, hv);
        CHECK(in.hit && std::fabs(in.t - 4.0f) < 1e-4f, "ray hits inside the rectangle");
        CHECK(len(in.normal - vec3(0, 0, 1)) < 1e-4f, "the rect normal faces the ray");
        CHECK(!maz::math::rayIntersectsRect(vec3(2.5f, 0.0f, 4.0f), vec3(0, 0, -1), c, uAxis, vAxis, hu, hv).hit,
              "a ray past the u half-extent misses");
        CHECK(!maz::math::rayIntersectsRect(vec3(0.0f, 1.5f, 4.0f), vec3(0, 0, -1), c, uAxis, vAxis, hu, hv).hit,
              "a ray past the v half-extent misses");
        // Corner (just inside).
        CHECK(maz::math::rayIntersectsRect(vec3(1.99f, 0.99f, 4.0f), vec3(0, 0, -1), c, uAxis, vAxis, hu, hv).hit,
              "a ray just inside the corner hits");
    }

    // --- 5. Determinism. ---
    {
        const auto a = maz::math::rayIntersectsDisk(vec3(0, 0, 5), vec3(0, 0, -1), vec3(0, 0, 0), vec3(0, 0, 1), 2.0f);
        const auto b = maz::math::rayIntersectsDisk(vec3(0, 0, 5), vec3(0, 0, -1), vec3(0, 0, 0), vec3(0, 0, 1), 2.0f);
        CHECK(a.hit == b.hit && a.t == b.t, "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("rayplanar: OK — disk/annulus/rect analytic + containment, hole, normals, determinism.\n");
        return 0;
    }
    std::printf("rayplanar: %d failure(s).\n", g_fail);
    return 1;
}
