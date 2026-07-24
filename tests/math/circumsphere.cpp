// tests/math/circumsphere.cpp — verifies the tetrahedron circumsphere (math Circumsphere.hpp).
// Ground truths, deterministic (fixed + seeded-LCG tetrahedra, no <random>, no clock):
//   * EQUIDISTANCE (airtight): the centre is equidistant from all four vertices, and that distance is the
//     reported radius — the defining property of the circumsphere;
//   * KNOWN VALUE: the regular tetrahedron {(1,1,1),(1,-1,-1),(-1,1,-1),(-1,-1,1)} has centre (0,0,0) and
//     circumradius sqrt(3);
//   * TRANSLATION INVARIANCE: translating the tetra translates the centre and leaves the radius unchanged;
//   * DEGENERACY: four coplanar points yield valid=false;
//   * IN-SPHERE PREDICATE: a point just inside/outside the sphere reads inside/outside correctly.
#include "maz/math/Circumsphere.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
};

int main() {
    using namespace maz::math;

    // --- 1. Equidistance over random non-degenerate tetrahedra. ---
    {
        Lcg rng{0xC15Fu};
        float worstEq = 0.0f;
        int tested = 0;
        for (int i = 0; i < 4000; ++i) {
            const vec3 a(rng.sym() * 5.0f, rng.sym() * 5.0f, rng.sym() * 5.0f);
            const vec3 b(rng.sym() * 5.0f, rng.sym() * 5.0f, rng.sym() * 5.0f);
            const vec3 c(rng.sym() * 5.0f, rng.sym() * 5.0f, rng.sym() * 5.0f);
            const vec3 d(rng.sym() * 5.0f, rng.sym() * 5.0f, rng.sym() * 5.0f);
            // Skip near-degenerate (small signed volume) tetrahedra.
            const vec3 ab = b - a, ac = c - a, ad = d - a;
            const float vol6 = std::fabs(ab.x * (ac.y * ad.z - ac.z * ad.y) - ab.y * (ac.x * ad.z - ac.z * ad.x) +
                                         ab.z * (ac.x * ad.y - ac.y * ad.x));
            if (vol6 < 5.0f) {
                continue; // too flat; conditioning is poor and not the point of this test
            }
            const Circumsphere s = circumsphere(a, b, c, d);
            CHECK(s.valid, "a non-degenerate tetra has a circumsphere");
            if (s.valid) {
                const float da = len(s.center - a), db = len(s.center - b);
                const float dc = len(s.center - c), dd = len(s.center - d);
                const float m = std::max(std::max(std::fabs(da - s.radius), std::fabs(db - s.radius)),
                                         std::max(std::fabs(dc - s.radius), std::fabs(dd - s.radius)));
                worstEq = std::max(worstEq, m / (s.radius + 1.0f));
                ++tested;
            }
        }
        CHECK(tested > 1500, "the random tetrahedra exercise many valid circumspheres");
        CHECK(worstEq < 1e-4f, "the centre is equidistant from all four vertices (= the radius)");
    }

    // --- 2. Regular tetrahedron: centre origin, radius sqrt(3). ---
    {
        const Circumsphere s = circumsphere(vec3(1, 1, 1), vec3(1, -1, -1), vec3(-1, 1, -1), vec3(-1, -1, 1));
        CHECK(s.valid, "regular tetra is valid");
        CHECK(len(s.center) < 1e-4f, "regular-tetra circumcentre is the origin");
        CHECK(std::fabs(s.radius - std::sqrt(3.0f)) < 1e-4f, "regular-tetra circumradius is sqrt(3)");
    }

    // --- 3. Translation invariance. ---
    {
        const vec3 a(0.2f, -0.4f, 1.1f), b(2.0f, 0.3f, -0.5f), c(-1.0f, 1.4f, 0.2f), d(0.5f, -1.2f, -1.3f);
        const vec3 t(10.0f, -7.0f, 3.5f);
        const Circumsphere s0 = circumsphere(a, b, c, d);
        const Circumsphere s1 = circumsphere(a + t, b + t, c + t, d + t);
        CHECK(s0.valid && s1.valid, "both tetrahedra are valid");
        CHECK(len((s0.center + t) - s1.center) < 1e-3f, "the centre translates with the tetra");
        CHECK(std::fabs(s0.radius - s1.radius) < 1e-3f, "the radius is translation-invariant");
    }

    // --- 4. Coplanar points are degenerate. ---
    {
        const Circumsphere s = circumsphere(vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), vec3(1, 1, 0));
        CHECK(!s.valid, "four coplanar points have no circumsphere");
    }

    // --- 5. In-sphere predicate. Unit-ish tetra; test a point near the centre and one far outside. ---
    {
        const vec3 a(1, 1, 1), b(1, -1, -1), c(-1, 1, -1), d(-1, -1, 1); // circumradius sqrt(3) ~ 1.732
        CHECK(insideCircumsphere(a, b, c, d, vec3(0, 0, 0)), "the centre is inside");
        CHECK(insideCircumsphere(a, b, c, d, vec3(1.6f, 0, 0)), "a point at r<sqrt(3) is inside");
        CHECK(!insideCircumsphere(a, b, c, d, vec3(2.0f, 0, 0)), "a point beyond the radius is outside");
        CHECK(!insideCircumsphere(a, b, c, d, vec3(5, 5, 5)), "a far point is outside");
    }

    if (g_fail == 0) {
        std::printf("circumsphere: OK — equidistance, regular tetra, translation, degeneracy, in-sphere.\n");
        return 0;
    }
    std::printf("circumsphere: %d failure(s).\n", g_fail);
    return 1;
}
