// tests/math/sdfops.cpp — verifies the SDF combination operators (math SdfOps.hpp).
// Ground truths, deterministic (fixed + seeded-LCG values, no <random>, no clock):
//   * HARD OPS (airtight identities): opUnion=min, opIntersect=max, opSubtract=max(a,-b);
//   * MODIFIERS: opRound(d,r)=d-r, opAnnular(d,r)=|d|-r, opInterpolate endpoints/midpoint;
//   * SMOOTH OPS: reduce to the hard op OUTSIDE the blend band (|a-b|>=k for union/intersect, |a+b|>=k for
//     subtract) and to the hard op when k<=0; smooth-union never exceeds min and dips at most k/4 below it at
//     a==b; symmetry of smooth-union;
//   * determinism.
#include "maz/math/SdfOps.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<std::uint32_t>(s >> 33); }
    float sym() { return static_cast<float>(next()) / 4294967296.0f * 10.0f - 5.0f; }
};

int main() {
    using namespace maz::math;

    // --- 1. Hard boolean identities over random pairs. ---
    {
        Lcg rng{0x50F7u};
        float worst = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            const float a = rng.sym(), b = rng.sym();
            worst = std::max(worst, std::fabs(opUnion(a, b) - std::min(a, b)));
            worst = std::max(worst, std::fabs(opIntersect(a, b) - std::max(a, b)));
            worst = std::max(worst, std::fabs(opSubtract(a, b) - std::max(a, -b)));
        }
        CHECK(worst < 1e-6f, "hard union/intersect/subtract equal min / max / max(a,-b)");
    }

    // --- 2. Modifiers. ---
    {
        CHECK(std::fabs(opRound(2.0f, 0.5f) - 1.5f) < 1e-6f, "opRound subtracts r");
        CHECK(std::fabs(opAnnular(-2.0f, 0.5f) - 1.5f) < 1e-6f, "opAnnular of an interior distance is |d|-r");
        CHECK(std::fabs(opAnnular(2.0f, 0.5f) - 1.5f) < 1e-6f, "opAnnular of an exterior distance is |d|-r");
        CHECK(std::fabs(opInterpolate(3.0f, 7.0f, 0.0f) - 3.0f) < 1e-6f, "morph at t=0 is a");
        CHECK(std::fabs(opInterpolate(3.0f, 7.0f, 1.0f) - 7.0f) < 1e-6f, "morph at t=1 is b");
        CHECK(std::fabs(opInterpolate(3.0f, 7.0f, 0.5f) - 5.0f) < 1e-6f, "morph at t=0.5 is the average");
    }

    // --- 3. Smooth ops reduce to hard ops outside the blend band and for k<=0. ---
    {
        const float k = 1.0f;
        Lcg rng{0x5EED3u};
        float worstU = 0.0f, worstI = 0.0f, worstS = 0.0f;
        int checkedU = 0, checkedS = 0;
        for (int i = 0; i < 20000; ++i) {
            const float a = rng.sym(), b = rng.sym();
            // Union/intersect: outside the band when |a-b| >= k.
            if (std::fabs(a - b) >= k) {
                worstU = std::max(worstU, std::fabs(opSmoothUnion(a, b, k) - std::min(a, b)));
                worstI = std::max(worstI, std::fabs(opSmoothIntersect(a, b, k) - std::max(a, b)));
                ++checkedU;
            }
            // Subtract: outside the band when |a+b| >= k.
            if (std::fabs(a + b) >= k) {
                worstS = std::max(worstS, std::fabs(opSmoothSubtract(a, b, k) - std::max(a, -b)));
                ++checkedS;
            }
        }
        CHECK(checkedU > 5000 && checkedS > 5000, "the random pairs exercise the outside-band regime");
        CHECK(worstU < 1e-5f, "smooth-union equals min outside the blend band");
        CHECK(worstI < 1e-5f, "smooth-intersect equals max outside the blend band");
        CHECK(worstS < 1e-5f, "smooth-subtract equals max(a,-b) outside the blend band");
        // k <= 0 reduces to the hard op.
        CHECK(opSmoothUnion(2.0f, 5.0f, 0.0f) == std::min(2.0f, 5.0f), "k<=0 smooth-union is min");
        CHECK(opSmoothIntersect(2.0f, 5.0f, -1.0f) == std::max(2.0f, 5.0f), "k<=0 smooth-intersect is max");
    }

    // --- 4. Smooth-union: never above min, dips exactly k/4 below at a==b; symmetric. ---
    {
        const float k = 2.0f;
        Lcg rng{0xB1E4u};
        float worstOver = 0.0f, worstUnder = 0.0f, worstSym = 0.0f;
        for (int i = 0; i < 20000; ++i) {
            const float a = rng.sym(), b = rng.sym();
            const float u = opSmoothUnion(a, b, k);
            worstOver = std::max(worstOver, u - std::min(a, b));               // should be <= 0
            worstUnder = std::max(worstUnder, std::min(a, b) - k * 0.25f - u); // u >= min - k/4
            worstSym = std::max(worstSym, std::fabs(u - opSmoothUnion(b, a, k)));
        }
        CHECK(worstOver < 1e-5f, "smooth-union never exceeds the hard min");
        CHECK(worstUnder < 1e-5f, "smooth-union dips at most k/4 below the hard min");
        CHECK(worstSym < 1e-5f, "smooth-union is symmetric in its arguments");
        CHECK(std::fabs(opSmoothUnion(3.0f, 3.0f, k) - (3.0f - k * 0.25f)) < 1e-5f,
              "at a==b the smooth-union indents by exactly k/4");
    }

    // --- 5. Determinism. ---
    {
        CHECK(opSmoothUnion(1.2f, -0.7f, 0.8f) == opSmoothUnion(1.2f, -0.7f, 0.8f),
              "identical inputs produce identical results");
    }

    if (g_fail == 0) {
        std::printf("sdfops: OK — hard ops, modifiers, smooth-band reduction, indent bound, determinism.\n");
        return 0;
    }
    std::printf("sdfops: %d failure(s).\n", g_fail);
    return 1;
}
