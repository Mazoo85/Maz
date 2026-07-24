// tests/math/colorlab.cpp — verifies CIE color science (math ColorLab.hpp).
// Ground truths, deterministic (fixed + seeded-LCG colors, no <random>, no clock):
//   * WHITE / BLACK (reference values): linear-RGB white maps to L*=100, a*=b*=0; black to (0,0,0);
//   * ROUND TRIP (airtight): linearRgbToLab -> labToLinearRgb recovers any in-gamut linear RGB; and the
//     XYZ matrix pair is a mutual inverse;
//   * NEUTRALS: grays have a* == b* == 0 and L* increases with luminance (monotone);
//   * DELTA-E: identical colors give 0; the metric is symmetric; a small perturbation gives a small positive
//     distance; larger color changes give larger Delta-E.
#include "maz/math/ColorLab.hpp"

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
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
};

int main() {
    using namespace maz::math;

    // --- 1. White and black reference values. ---
    {
        const vec3 white = linearRgbToLab(vec3(1, 1, 1));
        CHECK(std::fabs(white.x - 100.0f) < 1e-2f, "linear-RGB white has L* = 100");
        CHECK(std::fabs(white.y) < 1e-2f && std::fabs(white.z) < 1e-2f, "white is neutral (a*=b*=0)");
        const vec3 black = linearRgbToLab(vec3(0, 0, 0));
        CHECK(len(black) < 1e-3f, "black maps to L*a*b* (0,0,0)");
    }

    // --- 2. Round trips: RGB<->Lab and the XYZ matrix pair. ---
    {
        Lcg rng{0xC010u};
        float worstLab = 0.0f, worstXyz = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            const vec3 c(rng.unit(), rng.unit(), rng.unit());
            const vec3 back = labToLinearRgb(linearRgbToLab(c));
            worstLab = std::max(worstLab, len(back - c));
            const vec3 backX = xyzToLinearRgb(linearRgbToXyz(c));
            worstXyz = std::max(worstXyz, len(backX - c));
        }
        CHECK(worstXyz < 1e-4f, "linearRgb<->XYZ is a mutual inverse");
        CHECK(worstLab < 1e-3f, "linearRgb<->Lab round-trips for in-gamut colors");
    }

    // --- 3. Neutrals: grays are achromatic and L* is monotone in luminance. ---
    {
        float prevL = -1.0f;
        float worstAB = 0.0f;
        for (int i = 0; i <= 20; ++i) {
            const float g = static_cast<float>(i) / 20.0f;
            const vec3 lab = linearRgbToLab(vec3(g, g, g));
            worstAB = std::max(worstAB, std::fabs(lab.y) + std::fabs(lab.z));
            CHECK(lab.x >= prevL - 1e-4f, "L* is non-decreasing as gray brightens");
            prevL = lab.x;
        }
        CHECK(worstAB < 1e-2f, "grays are achromatic (a*=b*=0)");
    }

    // --- 4. Delta-E properties. ---
    {
        const vec3 red = linearRgbToLab(vec3(1, 0, 0));
        const vec3 grn = linearRgbToLab(vec3(0, 1, 0));
        const vec3 blu = linearRgbToLab(vec3(0, 0, 1));
        CHECK(deltaE76(red, red) < 1e-4f, "Delta-E of a color with itself is 0");
        CHECK(std::fabs(deltaE76(red, grn) - deltaE76(grn, red)) < 1e-4f, "Delta-E is symmetric");
        // A small perturbation is a positive distance, smaller than the red-vs-green gap.
        const vec3 redish = linearRgbToLab(vec3(0.98f, 0.02f, 0.0f));
        const float small = deltaE76(red, redish);
        CHECK(small > 0.0f && small < deltaE76(red, grn), "a small color change gives a positive, sub-primary Delta-E");
        CHECK(deltaE76(red, grn) > 50.0f && deltaE76(red, blu) > 50.0f, "distinct primaries are far apart");
    }

    if (g_fail == 0) {
        std::printf("colorlab: OK — white/black, round trips, neutrals, Delta-E.\n");
        return 0;
    }
    std::printf("colorlab: %d failure(s).\n", g_fail);
    return 1;
}
