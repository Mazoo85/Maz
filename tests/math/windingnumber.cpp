// tests/math/meshwinding.cpp — verifies the generalized winding number point-in-mesh test (MeshWinding.hpp).
// Ground truths, deterministic (fixed cube mesh + seeded LCG, no <random>, no clock):
//   * interior points read |winding| ~ 1, exterior points ~ 0 (the defining property);
//   * pointInMesh classifies hundreds of random interior/exterior points correctly against the cube's box;
//   * ROBUSTNESS: after deleting a triangle (punching a hole), interior points STILL read inside — the key
//     advantage of the generalized winding number over ray-parity, which would leak through the hole;
//   * a far-away point reads ~0;
//   * determinism.
#include "maz/math/WindingNumber.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    float range(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(next() % 100000u) / 99999.0f); }
};

// Unit cube [-1,1]^3, outward-facing CCW winding, 12 triangles.
static void makeCube(std::vector<vec3>& v, std::vector<std::uint32_t>& idx) {
    v = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
         {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    idx = {0, 2, 1, 0, 3, 2,  // -Z
           4, 5, 6, 4, 6, 7,  // +Z
           0, 7, 3, 0, 4, 7,  // -X
           1, 2, 6, 1, 6, 5,  // +X
           0, 1, 5, 0, 5, 4,  // -Y
           3, 7, 6, 3, 6, 2}; // +Y
}

int main() {
    std::vector<vec3> v;
    std::vector<std::uint32_t> idx;
    makeCube(v, idx);

    // --- 1. Center inside (~1), far point outside (~0). ---
    {
        const float wc = std::fabs(maz::math::windingNumber(v, idx, vec3(0, 0, 0)));
        const float wf = std::fabs(maz::math::windingNumber(v, idx, vec3(10, 10, 10)));
        CHECK(std::fabs(wc - 1.0f) < 1e-2f, "the cube centre has winding number ~1 (inside)");
        CHECK(wf < 1e-2f, "a far point has winding number ~0 (outside)");
        CHECK(maz::math::pointInMesh(v, idx, vec3(0, 0, 0)), "the centre is classified inside");
        CHECK(!maz::math::pointInMesh(v, idx, vec3(10, 10, 10)), "the far point is classified outside");
    }

    // --- 2. Random interior / exterior classification. ---
    {
        Lcg rng{0x51DEu};
        bool ok = true;
        int inN = 0, outN = 0;
        for (int i = 0; i < 400; ++i) {
            const vec3 p(rng.range(-3, 3), rng.range(-3, 3), rng.range(-3, 3));
            // Ground truth from the axis-aligned box, with a margin to avoid surface ambiguity.
            const bool insideBox = std::fabs(p.x) < 0.9f && std::fabs(p.y) < 0.9f && std::fabs(p.z) < 0.9f;
            const bool outsideBox = std::fabs(p.x) > 1.1f || std::fabs(p.y) > 1.1f || std::fabs(p.z) > 1.1f;
            if (!insideBox && !outsideBox) continue; // skip near-surface band
            const bool got = maz::math::pointInMesh(v, idx, p);
            if (insideBox) { ++inN; if (!got) ok = false; }
            if (outsideBox) { ++outN; if (got) ok = false; }
        }
        CHECK(inN > 5 && outN > 100, "sampled enough interior and exterior points");
        CHECK(ok, "pointInMesh classifies interior and exterior points correctly");
    }

    // --- 3. Robustness: a hole doesn't leak the inside test. ---
    {
        std::vector<std::uint32_t> holed = idx;
        holed.resize(holed.size() - 3); // drop the last triangle (open the +Y face partially)
        // Interior points still read strongly inside.
        const float wc = std::fabs(maz::math::windingNumber(v, holed, vec3(0, 0, 0)));
        CHECK(wc > 0.5f, "an interior point still reads inside after punching a hole (graceful degradation)");
        CHECK(maz::math::pointInMesh(v, holed, vec3(0.0f, -0.3f, 0.0f)),
              "pointInMesh still holds inside despite the hole");
        // Far exterior still outside.
        CHECK(!maz::math::pointInMesh(v, holed, vec3(8, 8, 8)), "exterior stays outside with the hole");
    }

    // --- 4. Determinism. ---
    {
        const float a = maz::math::windingNumber(v, idx, vec3(0.2f, -0.4f, 0.1f));
        const float b = maz::math::windingNumber(v, idx, vec3(0.2f, -0.4f, 0.1f));
        CHECK(a == b, "identical inputs produce identical winding numbers");
    }

    if (g_fail == 0) {
        std::printf("meshwinding: OK — inside/outside, classification, hole robustness, determinism.\n");
        return 0;
    }
    std::printf("meshwinding: %d failure(s).\n", g_fail);
    return 1;
}
