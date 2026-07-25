// tests/render/meshraybvh.cpp — verifies the per-mesh triangle ray BVH (render::MeshRayBvh). A correct
// acceleration structure must return EXACTLY what brute force returns (same hit/miss, same distance) while
// visiting far fewer triangles. The reference is a brute-force nearest-hit over every triangle using the SAME
// Möller–Trumbore (detail::bvhRayTri), so any discrepancy is purely the BVH traversal, not the intersection
// math. Test mesh is a closed icosphere (~1280 tris). Covered: nearest-hit parity for hitting/missing/inside
// rays, measured triangle-test reduction (the actual speedup), occluded() parity, tMax honored, empty-mesh
// safety, and determinism. Pure CPU, headless.
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshRayBvh.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;
namespace math = maz::math;

// Brute-force nearest forward hit over all triangles, using the same ray/tri as the BVH.
static MeshRayHit bruteHit(const shapes::MeshData& m, const math::vec3& o, const math::vec3& d, float maxDist,
                           std::size_t* tested) {
    MeshRayHit best;
    best.t = maxDist;
    std::size_t n = 0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const math::vec3 a(m.vertices[m.indices[t]].px, m.vertices[m.indices[t]].py, m.vertices[m.indices[t]].pz);
        const math::vec3 b(m.vertices[m.indices[t + 1]].px, m.vertices[m.indices[t + 1]].py,
                           m.vertices[m.indices[t + 1]].pz);
        const math::vec3 c(m.vertices[m.indices[t + 2]].px, m.vertices[m.indices[t + 2]].py,
                           m.vertices[m.indices[t + 2]].pz);
        float tt, u, v;
        ++n;
        if (detail::bvhRayTri(o, d, a, b, c, best.t, tt, u, v)) {
            best.hit = true;
            best.t = tt;
            best.triangle = static_cast<std::uint32_t>(t / 3);
        }
    }
    if (tested) *tested = n;
    return best;
}

// Deterministic unit vector from an integer seed (no RNG, no <random>) — a cheap hash into a direction.
static math::vec3 dirFromSeed(std::uint32_t s) {
    s = s * 747796405u + 2891336453u;
    std::uint32_t w = ((s >> ((s >> 28) + 4)) ^ s) * 277803737u;
    w = (w >> 22) ^ w;
    const float a = static_cast<float>(w & 0xffffu) / 65535.0f * 6.2831853f;      // azimuth
    const float z = static_cast<float>((w >> 16) & 0xffffu) / 65535.0f * 2.0f - 1.0f; // cos(polar)
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return math::vec3(r * std::cos(a), r * std::sin(a), z);
}

int main() {
    const shapes::MeshData sphere = makeIcosphere(2.0f, 3); // ~1280 triangles, closed
    const std::size_t triN = sphere.indices.size() / 3;
    CHECK(triN > 500, "test mesh is reasonably tessellated");

    MeshRayBvh bvh(sphere);
    CHECK(!bvh.empty(), "BVH built");
    CHECK(bvh.triangleCount() == triN, "BVH indexes every triangle");
    CHECK(bvh.nodeCount() >= 3, "BVH has interior structure (not a single leaf)");

    // --- 1. Nearest-hit parity + measured speedup over many rays aimed at the sphere from outside. ---
    {
        std::size_t bvhTests = 0, bruteTests = 0;
        int hits = 0, mismatches = 0, triMismatch = 0;
        const int N = 400;
        for (int i = 0; i < N; ++i) {
            const math::vec3 dir = dirFromSeed(static_cast<std::uint32_t>(i) + 1u);
            const math::vec3 origin = dir * 6.0f;   // outside the r=2 sphere
            const math::vec3 toCenter = math::normalize(-origin); // aims through the middle → guaranteed hit
            std::size_t tb = 0, tr = 0;
            const MeshRayHit hb = bvh.intersect(origin, toCenter, 100.0f, &tb);
            const MeshRayHit hr = bruteHit(sphere, origin, toCenter, 100.0f, &tr);
            bvhTests += tb;
            bruteTests += tr;
            if (hb.hit != hr.hit) ++mismatches;
            if (hb.hit && hr.hit) {
                ++hits;
                if (std::fabs(hb.t - hr.t) > 1e-3f) ++mismatches;
                if (hb.triangle != hr.triangle) ++triMismatch;
                // Reported hit point must sit on the ray at distance t.
                const math::vec3 expect = origin + toCenter * hb.t;
                const math::vec3 diff = hb.point - expect;
                if (std::sqrt(math::dot(diff, diff)) > 1e-3f) ++mismatches;
            }
        }
        CHECK(hits > N - 5, "aimed rays hit the sphere");
        CHECK(mismatches == 0, "BVH nearest-hit distance matches brute force exactly");
        CHECK(triMismatch == 0, "BVH reports the same triangle as brute force");
        // The whole point: far fewer triangle tests than brute force.
        CHECK(bvhTests * 4 < bruteTests, "BVH tests << brute force (>4x fewer triangle intersections)");
        std::printf("  parity: %d hits, bvh tri-tests=%zu vs brute=%zu (%.1fx fewer)\n", hits, bvhTests,
                    bruteTests, static_cast<double>(bruteTests) / static_cast<double>(bvhTests ? bvhTests : 1));
    }

    // --- 2. Rays pointing AWAY from the sphere miss (both BVH and brute). ---
    {
        int agree = 0;
        for (int i = 0; i < 100; ++i) {
            const math::vec3 dir = dirFromSeed(static_cast<std::uint32_t>(i) * 3u + 7u);
            const math::vec3 origin = dir * 6.0f;
            const math::vec3 away = math::normalize(origin); // points outward → misses
            const MeshRayHit hb = bvh.intersect(origin, away, 100.0f);
            const MeshRayHit hr = bruteHit(sphere, origin, away, 100.0f, nullptr);
            if (hb.hit == hr.hit && !hb.hit) ++agree;
        }
        CHECK(agree == 100, "rays pointing away miss under both BVH and brute force");
    }

    // --- 3. Rays from INSIDE the sphere hit the far wall, matching brute force. ---
    {
        int mism = 0;
        for (int i = 0; i < 100; ++i) {
            const math::vec3 dir = dirFromSeed(static_cast<std::uint32_t>(i) * 5u + 11u);
            const MeshRayHit hb = bvh.intersect(math::vec3(0.0f), dir, 100.0f);
            const MeshRayHit hr = bruteHit(sphere, math::vec3(0.0f), dir, 100.0f, nullptr);
            if (hb.hit != hr.hit || (hb.hit && std::fabs(hb.t - hr.t) > 1e-3f)) ++mism;
        }
        CHECK(mism == 0, "rays from inside hit the far wall, matching brute force");
    }

    // --- 4. occluded() agrees with brute-force any-hit, including the tMax cutoff. ---
    {
        int mism = 0;
        for (int i = 0; i < 200; ++i) {
            const math::vec3 dir = dirFromSeed(static_cast<std::uint32_t>(i) * 2u + 3u);
            const math::vec3 origin = dir * 6.0f;
            const math::vec3 toCenter = math::normalize(-origin);
            // Full distance: should be occluded (something blocks it). Short distance: sphere is >4 units away,
            // so a 3-unit ray from radius-6 never reaches it → not occluded.
            const bool occFull = bvh.occluded(origin, toCenter, 100.0f);
            const bool occShort = bvh.occluded(origin, toCenter, 3.0f);
            const bool refFull = bruteHit(sphere, origin, toCenter, 100.0f, nullptr).hit;
            const bool refShort = bruteHit(sphere, origin, toCenter, 3.0f, nullptr).hit;
            if (occFull != refFull || occShort != refShort) ++mism;
        }
        CHECK(mism == 0, "occluded() matches brute any-hit and honors tMax");
    }

    // --- 5. intersect() honors maxDist: a hit beyond the limit is reported as a miss. ---
    {
        const math::vec3 origin(6.0f, 0.0f, 0.0f);
        const math::vec3 toCenter(-1.0f, 0.0f, 0.0f);
        const MeshRayHit full = bvh.intersect(origin, toCenter, 100.0f);
        CHECK(full.hit && full.t > 3.5f && full.t < 4.5f, "front wall hit near t=4");
        const MeshRayHit clipped = bvh.intersect(origin, toCenter, 3.0f);
        CHECK(!clipped.hit, "hit beyond maxDist is a miss");
    }

    // --- 6. Empty / degenerate meshes are safe and always miss. ---
    {
        MeshRayBvh empty{shapes::MeshData{}};
        CHECK(empty.empty(), "empty mesh yields empty BVH");
        CHECK(!empty.intersect(math::vec3(0.0f), math::vec3(1, 0, 0)).hit, "empty BVH never hits");
        CHECK(!empty.occluded(math::vec3(0.0f), math::vec3(1, 0, 0), 100.0f), "empty BVH never occludes");
    }

    // --- 7. Deterministic: rebuilding gives an identical structure and identical query results. ---
    {
        MeshRayBvh a(sphere), b(sphere);
        bool same = a.nodeCount() == b.nodeCount() && a.triangleCount() == b.triangleCount();
        for (int i = 0; i < 100 && same; ++i) {
            const math::vec3 dir = dirFromSeed(static_cast<std::uint32_t>(i) + 99u);
            const math::vec3 origin = dir * 6.0f;
            const math::vec3 toCenter = math::normalize(-origin);
            const MeshRayHit ha = a.intersect(origin, toCenter, 100.0f);
            const MeshRayHit hb = b.intersect(origin, toCenter, 100.0f);
            if (ha.hit != hb.hit || ha.triangle != hb.triangle || ha.t != hb.t) same = false;
        }
        CHECK(same, "BVH build and queries are deterministic");
    }

    if (g_fail == 0) {
        std::printf("meshraybvh: OK — exact parity with brute force, measured triangle-test reduction.\n");
        return 0;
    }
    std::printf("meshraybvh: %d failure(s).\n", g_fail);
    return 1;
}
