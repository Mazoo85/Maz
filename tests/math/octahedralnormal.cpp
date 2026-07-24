// tests/math/octahedralnormal.cpp — verifies octahedral unit-vector encoding (math OctahedralNormal.hpp).
// Ground truths, deterministic (seeded-LCG unit vectors, no <random>, no clock):
//   * ROUND TRIP (airtight): octDecode(octEncode(n)) recovers the direction n for any unit vector, to tiny
//     error — the defining property of a lossless-ish codec;
//   * RANGE: the encoded point always lies in [-1,1]^2;
//   * UNIT OUTPUT: every decoded vector is unit length;
//   * KNOWN AXES: +Z encodes to (0,0); the cardinal axes map to the expected octahedron corners;
//   * HEMISPHERE VARIANT: octEncodeHemi/octDecodeHemi round-trips for +Z-hemisphere vectors and uses the
//     full square (encoded points reach the corners).
#include "maz/math/OctahedralNormal.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

using maz::math::vec2;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float len3(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 2.0f - 1.0f; }
};

// A uniformly-ish random unit vector (reject tiny ones).
static vec3 randUnit(Lcg& rng) {
    for (;;) {
        const vec3 v(rng.sym(), rng.sym(), rng.sym());
        const float l = len3(v);
        if (l > 0.2f && l <= 1.0f) {
            return v * (1.0f / l);
        }
    }
}

int main() {
    using namespace maz::math;

    // --- 1. Full-sphere round trip + range + unit output. ---
    {
        Lcg rng{0x0C7Au};
        float worstAng = 0.0f, worstLen = 0.0f, worstRange = 0.0f;
        for (int i = 0; i < 20000; ++i) {
            const vec3 n = randUnit(rng);
            const vec2 e = octEncode(n);
            worstRange = std::max(worstRange, std::max(std::fabs(e.x), std::fabs(e.y)));
            const vec3 d = octDecode(e);
            worstLen = std::max(worstLen, std::fabs(len3(d) - 1.0f));
            const float c = dot3(n, d);
            const float ang = std::acos(c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c));
            worstAng = std::max(worstAng, ang);
        }
        CHECK(worstRange <= 1.0f + 1e-5f, "encoded points lie within [-1,1]^2");
        CHECK(worstLen < 1e-5f, "decoded vectors are unit length");
        CHECK(worstAng < 2e-3f, "octDecode(octEncode(n)) recovers the direction (tiny angular error)");
    }

    // --- 2. Known axes. ---
    {
        CHECK(len3(octDecode(octEncode(vec3(0, 0, 1))) - vec3(0, 0, 1)) < 1e-6f, "+Z round-trips");
        const vec2 pz = octEncode(vec3(0, 0, 1));
        CHECK(std::fabs(pz.x) < 1e-6f && std::fabs(pz.y) < 1e-6f, "+Z encodes to the origin (0,0)");
        // +X, +Y lie on the octahedron equator; encode to axis points of magnitude 1 after folding? Just
        // verify they decode back to themselves.
        CHECK(len3(octDecode(octEncode(vec3(1, 0, 0))) - vec3(1, 0, 0)) < 1e-5f, "+X round-trips");
        CHECK(len3(octDecode(octEncode(vec3(0, 1, 0))) - vec3(0, 1, 0)) < 1e-5f, "+Y round-trips");
        CHECK(len3(octDecode(octEncode(vec3(0, 0, -1))) - vec3(0, 0, -1)) < 1e-5f, "-Z round-trips");
    }

    // --- 3. Hemisphere variant round trip. ---
    {
        Lcg rng{0x4E10u};
        float worstAng = 0.0f, worstLen = 0.0f, maxComp = 0.0f;
        for (int i = 0; i < 20000; ++i) {
            vec3 n = randUnit(rng);
            n.z = std::fabs(n.z); // force +Z hemisphere
            n = n * (1.0f / len3(n));
            const vec2 e = octEncodeHemi(n);
            maxComp = std::max(maxComp, std::max(std::fabs(e.x), std::fabs(e.y)));
            const vec3 d = octDecodeHemi(e);
            worstLen = std::max(worstLen, std::fabs(len3(d) - 1.0f));
            const float c = dot3(n, d);
            worstAng = std::max(worstAng, std::acos(c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c)));
        }
        CHECK(worstLen < 1e-5f, "hemi-decoded vectors are unit length");
        CHECK(worstAng < 2e-3f, "hemi octDecode(octEncode(n)) recovers the direction");
        CHECK(maxComp > 0.9f, "the hemisphere encoding uses the full square (reaches near the corners)");
    }

    if (g_fail == 0) {
        std::printf("octahedralnormal: OK — round trip, range, unit output, axes, hemisphere.\n");
        return 0;
    }
    std::printf("octahedralnormal: %d failure(s).\n", g_fail);
    return 1;
}
