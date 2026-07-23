// tests/math/tetrahedron.cpp — verifies tetrahedron utilities (math Tetrahedron.hpp).
// Ground truths, deterministic (fixed + seeded-LCG points, no <random>, no clock):
//   * BARYCENTRIC RECONSTRUCTION (airtight): the four weights sum to 1 and rebuild the point exactly,
//     p = w0*a + w1*b + w2*c + w3*d; each vertex has a unit basis weight;
//   * CONTAINS: a point is inside iff all four weights are >= 0 (cross-checked against the min weight sign);
//     the centroid is inside, a far point is outside;
//   * SIGNED VOLUME: the unit corner tetra has volume 1/6, and the sign flips when two vertices are swapped;
//   * CLOSEST POINT: inside points map to themselves; outside points map onto the surface and are the true
//     nearest point (checked against a brute-force sampling of the four faces);
//   * determinism.
#include "maz/math/Tetrahedron.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;
using maz::math::dot;
using maz::math::closestPointOnTriangle;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(dot(v, v)); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 4.0f - 2.0f; }
};

int main() {
    // A generic (non-degenerate) tetrahedron.
    const vec3 a(0.2f, -0.1f, 0.3f), b(2.1f, 0.4f, -0.2f), c(-0.3f, 1.9f, 0.5f), d(0.6f, 0.2f, 2.4f);

    // --- 1. Barycentric reconstruction + sum-to-one + vertex basis. ---
    {
        Lcg rng{0x7E7Au};
        float worstSum = 0.0f, worstRec = 0.0f;
        for (int i = 0; i < 3000; ++i) {
            const vec3 p(rng.sym(), rng.sym(), rng.sym());
            const auto bc = maz::math::barycentricTetrahedron(p, a, b, c, d);
            const float sum = bc.w[0] + bc.w[1] + bc.w[2] + bc.w[3];
            worstSum = std::max(worstSum, std::fabs(sum - 1.0f));
            const vec3 rec = a * bc.w[0] + b * bc.w[1] + c * bc.w[2] + d * bc.w[3];
            worstRec = std::max(worstRec, len(rec - p));
        }
        CHECK(worstSum < 1e-4f, "the four barycentric weights sum to 1");
        CHECK(worstRec < 1e-3f, "the weights reconstruct the point exactly");
        const auto ba = maz::math::barycentricTetrahedron(a, a, b, c, d);
        CHECK(std::fabs(ba.w[0] - 1.0f) < 1e-4f && std::fabs(ba.w[1]) < 1e-4f, "vertex a has weight (1,0,0,0)");
    }

    // --- 2. Contains matches the all-weights-nonnegative test; centroid in, far point out. ---
    {
        const vec3 centroid = (a + b + c + d) * 0.25f;
        CHECK(maz::math::tetrahedronContains(centroid, a, b, c, d), "the centroid is inside");
        CHECK(!maz::math::tetrahedronContains(vec3(100, 100, 100), a, b, c, d), "a far point is outside");
        Lcg rng{0x1234u};
        // Sample within the tetra's bounding box so a good fraction land inside (a tetra fills ~1/3 of its
        // AABB); points across a huge cube would almost never hit the small tetra.
        const vec3 lo(std::min(std::min(a.x, b.x), std::min(c.x, d.x)),
                      std::min(std::min(a.y, b.y), std::min(c.y, d.y)),
                      std::min(std::min(a.z, b.z), std::min(c.z, d.z)));
        const vec3 hi(std::max(std::max(a.x, b.x), std::max(c.x, d.x)),
                      std::max(std::max(a.y, b.y), std::max(c.y, d.y)),
                      std::max(std::max(a.z, b.z), std::max(c.z, d.z)));
        int mismatch = 0, inside = 0;
        for (int i = 0; i < 5000; ++i) {
            const float ux = (rng.sym() + 2.0f) * 0.25f, uy = (rng.sym() + 2.0f) * 0.25f, uz = (rng.sym() + 2.0f) * 0.25f;
            const vec3 p(lo.x + (hi.x - lo.x) * ux, lo.y + (hi.y - lo.y) * uy, lo.z + (hi.z - lo.z) * uz);
            const auto bc = maz::math::barycentricTetrahedron(p, a, b, c, d);
            const float mn = std::min(std::min(bc.w[0], bc.w[1]), std::min(bc.w[2], bc.w[3]));
            const bool contains = maz::math::tetrahedronContains(p, a, b, c, d, 0.0f);
            if (contains) ++inside;
            if (contains != (mn >= 0.0f)) ++mismatch;
        }
        CHECK(inside > 50, "some random points land inside");
        CHECK(mismatch == 0, "contains() agrees with the all-weights-nonnegative test");
    }

    // --- 3. Signed volume: unit corner tetra = 1/6; sign flips on a vertex swap. ---
    {
        const vec3 o(0, 0, 0), x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);
        const float v = maz::math::tetrahedronVolume(o, x, y, z);
        CHECK(std::fabs(v - 1.0f / 6.0f) < 1e-6f, "the unit corner tetrahedron has volume 1/6");
        const float vswap = maz::math::tetrahedronVolume(o, y, x, z); // swap two vertices
        CHECK(std::fabs(vswap + v) < 1e-6f, "swapping two vertices flips the volume sign");
    }

    // --- 4. Closest point: inside -> itself; outside -> nearest surface point (brute-force check). ---
    {
        Lcg rng{0xC105u};
        float worst = 0.0f;
        int outsideChecked = 0;
        const vec3 faces[4][3] = {{a, b, c}, {a, b, d}, {a, c, d}, {b, c, d}};
        for (int i = 0; i < 4000; ++i) {
            const vec3 p(rng.sym() * 1.5f, rng.sym() * 1.5f, rng.sym() * 1.5f);
            const vec3 q = maz::math::closestPointTetrahedron(p, a, b, c, d);
            if (maz::math::tetrahedronContains(p, a, b, c, d, 0.0f)) {
                CHECK(len(q - p) < 1e-4f, "an interior point is its own closest point");
                continue;
            }
            ++outsideChecked;
            // Brute-force nearest over the four faces.
            vec3 brute = closestPointOnTriangle(p, faces[0][0], faces[0][1], faces[0][2]);
            float bd = len(p - brute);
            for (int f = 1; f < 4; ++f) {
                const vec3 cp = closestPointOnTriangle(p, faces[f][0], faces[f][1], faces[f][2]);
                const float dd = len(p - cp);
                if (dd < bd) { bd = dd; brute = cp; }
            }
            worst = std::max(worst, std::fabs(len(p - q) - bd));
        }
        CHECK(outsideChecked > 1000, "many points fall outside");
        CHECK(worst < 1e-4f, "the closest surface point matches the brute-force nearest face point");
    }

    // --- 5. Determinism. ---
    {
        const auto b1 = maz::math::barycentricTetrahedron(vec3(0.5f, 0.5f, 0.5f), a, b, c, d);
        const auto b2 = maz::math::barycentricTetrahedron(vec3(0.5f, 0.5f, 0.5f), a, b, c, d);
        CHECK(b1.w[0] == b2.w[0] && b1.w[3] == b2.w[3], "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("tetrahedron: OK — barycentric, contains, signed volume, closest point, determinism.\n");
        return 0;
    }
    std::printf("tetrahedron: %d failure(s).\n", g_fail);
    return 1;
}
