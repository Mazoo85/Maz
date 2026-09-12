// tests/math/orthonormalbasis.cpp — verifies the branchless orthonormal-basis builder
// (math::orthonormalBasis / fromLocal). Ground truths over many unit normals (axis-aligned, the Duff
// z=-1 danger case, and a deterministic random sweep): tangent/bitangent/normal are mutually orthogonal and
// unit length; the frame is RIGHT-handed (cross(tangent,bitangent) == normal); fromLocal maps local +Z to
// the normal and +X to the tangent, and preserves length; a degenerate (zero) normal falls back to the
// canonical axis frame; and a z-up cosine-hemisphere sample rotated by the basis lands in the normal's
// hemisphere (integration with Sampling.hpp). Pure CPU, deterministic (LCG, no <random>/clock).
#include "maz/math/OrthonormalBasis.hpp"
#include "maz/math/Sampling.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::vec3;
using maz::math::Basis3;
using maz::math::orthonormalBasis;
using maz::math::fromLocal;
namespace mm = maz::math;

static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
static float len(const vec3& v) { return std::sqrt(mm::dot(v, v)); }

static void checkFrame(const vec3& n, const char* what) {
    const Basis3 b = orthonormalBasis(n);
    CHECK(near(len(b.tangent), 1.0f) && near(len(b.bitangent), 1.0f) && near(len(b.normal), 1.0f),
          what); // unit length
    CHECK(near(mm::dot(b.tangent, b.bitangent), 0.0f) && near(mm::dot(b.tangent, b.normal), 0.0f) &&
              near(mm::dot(b.bitangent, b.normal), 0.0f),
          what); // mutually orthogonal
    const vec3 c = mm::cross(b.tangent, b.bitangent);
    CHECK(near(c.x, n.x) && near(c.y, n.y) && near(c.z, n.z), what); // right-handed: t x b == n
    CHECK(near(b.normal.x, n.x) && near(b.normal.y, n.y) && near(b.normal.z, n.z), what); // normal == n
}

struct Lcg {
    std::uint64_t s;
    float next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>((s >> 40) & 0xFFFFFF) / 16777216.0f;
    }
};

int main() {
    // --- 1. Axis-aligned normals, incl. the z = -1 case that trips naive builders. ---
    checkFrame(vec3(1, 0, 0), "basis about +x");
    checkFrame(vec3(0, 1, 0), "basis about +y");
    checkFrame(vec3(0, 0, 1), "basis about +z");
    checkFrame(vec3(0, 0, -1), "basis about -z (Duff danger case)");
    checkFrame(vec3(-1, 0, 0), "basis about -x");

    // --- 2. A deterministic sweep of random unit normals. ---
    {
        Lcg rng{0xC0FFEEu};
        for (int i = 0; i < 500; ++i) {
            vec3 n(rng.next() * 2.0f - 1.0f, rng.next() * 2.0f - 1.0f, rng.next() * 2.0f - 1.0f);
            const float l = len(n);
            if (l < 1e-3f) continue;
            n = n / l;
            checkFrame(n, "basis about random unit normal");
        }
    }

    // --- 3. fromLocal maps the local axes correctly and preserves length. ---
    {
        const Basis3 b = orthonormalBasis(mm::normalize(vec3(0.3f, 0.6f, -0.2f)));
        const vec3 z = fromLocal(b, vec3(0, 0, 1));
        CHECK(near(z.x, b.normal.x) && near(z.y, b.normal.y) && near(z.z, b.normal.z), "local +Z -> normal");
        const vec3 x = fromLocal(b, vec3(1, 0, 0));
        CHECK(near(x.x, b.tangent.x) && near(x.y, b.tangent.y) && near(x.z, b.tangent.z), "local +X -> tangent");
        const vec3 v(0.4f, -0.7f, 0.5f);
        CHECK(near(len(fromLocal(b, v)), len(v)), "fromLocal preserves length");
    }

    // --- 4. Degenerate (zero) normal -> canonical axis frame. ---
    {
        const Basis3 b = orthonormalBasis(vec3(0, 0, 0));
        CHECK(near(b.tangent.x, 1.0f) && near(b.bitangent.y, 1.0f) && near(b.normal.z, 1.0f),
              "zero normal -> canonical frame");
    }

    // --- 5. Integration: a cosine-hemisphere sample oriented to a normal stays in that hemisphere. ---
    {
        Lcg rng{0x5EEDu};
        const vec3 n = mm::normalize(vec3(0.2f, -0.9f, 0.3f));
        const Basis3 b = orthonormalBasis(n);
        int bad = 0;
        for (int i = 0; i < 2000; ++i) {
            const vec3 local = mm::sampleCosineHemisphere(rng.next(), rng.next()); // z-up
            const vec3 world = fromLocal(b, local);
            if (mm::dot(world, n) < -1e-4f) ++bad;
        }
        CHECK(bad == 0, "oriented cosine-hemisphere samples stay in the normal's hemisphere");
    }

    if (g_fail == 0) {
        std::printf("orthonormal basis: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
