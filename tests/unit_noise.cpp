// Unit tests for maz::math::PerlinNoise — Ken Perlin's improved gradient noise (2D/3D)
// plus fBm, seeded via maz::core::Rng. The definitive correctness anchor is that the
// field is exactly 0 at integer lattice coordinates (a broken fade/grad/lerp/index
// would break it). Also: same seed reproduces bit-identical values, different seeds
// differ, output is bounded ~[-1,1] and actually varies, noise2D delegates to the z=0
// slice of noise3D, and fbm2D(octaves==1) reproduces noise2D. Pure C++, no GPU/display.

#include "maz/math/Noise.hpp"

#include <cstdio>
#include <cmath>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }
bool approxTight(float a, float b) { return std::fabs(a - b) < 1e-6f; }

} // namespace

int main() {
    // --- 1. LATTICE-ZERO (THE EXACT ANCHOR) ----------------------------------
    // At integer coords the fractional parts are 0, so the fade weights select the
    // origin corner whose offset is (0,0,0) -> noise is exactly 0.
    {
        PerlinNoise n(1234);
        check(approxTight(n.noise2D(0.0f, 0.0f), 0.0f), "noise2D(0,0) == 0");
        check(approxTight(n.noise2D(1.0f, 0.0f), 0.0f), "noise2D(1,0) == 0");
        check(approxTight(n.noise2D(0.0f, 1.0f), 0.0f), "noise2D(0,1) == 0");
        check(approxTight(n.noise2D(5.0f, 5.0f), 0.0f), "noise2D(5,5) == 0");
        check(approxTight(n.noise2D(-3.0f, 2.0f), 0.0f), "noise2D(-3,2) == 0");
        check(approxTight(n.noise2D(-1.0f, -1.0f), 0.0f), "noise2D(-1,-1) == 0");
        check(approxTight(n.noise3D(0.0f, 0.0f, 0.0f), 0.0f), "noise3D(0,0,0) == 0");
        check(approxTight(n.noise3D(1.0f, 2.0f, 3.0f), 0.0f), "noise3D(1,2,3) == 0");
        check(approxTight(n.noise3D(-1.0f, -1.0f, -1.0f), 0.0f), "noise3D(-1,-1,-1) == 0");
    }

    // --- 2. DETERMINISM, SAME SEED -------------------------------------------
    {
        PerlinNoise a(42), b(42);
        check(a.noise2D(0.5f, 0.5f) == b.noise2D(0.5f, 0.5f), "same seed: noise2D(0.5,0.5) identical");
        check(a.noise2D(1.3f, -2.7f) == b.noise2D(1.3f, -2.7f), "same seed: noise2D(1.3,-2.7) identical");
        check(a.noise2D(10.1f, 3.9f) == b.noise2D(10.1f, 3.9f), "same seed: noise2D(10.1,3.9) identical");
    }

    // --- 3. DIFFERENT SEED DIFFERS -------------------------------------------
    {
        PerlinNoise a(1), c(999);
        check(a.noise2D(0.5f, 0.5f) != c.noise2D(0.5f, 0.5f), "different seed: noise2D(0.5,0.5) differs");
    }

    // --- 4. RANGE BOUND + VARIATION ------------------------------------------
    {
        PerlinNoise n(7);
        bool inBounds = true;
        float maxAbs = 0.0f;
        for (float x = 0.0f; x <= 8.0f; x += 0.25f) {
            for (float y = 0.0f; y <= 8.0f; y += 0.25f) {
                float val = n.noise2D(x, y);
                if (val < -1.001f || val > 1.001f) {
                    inBounds = false;
                }
                maxAbs = std::fmax(maxAbs, std::fabs(val));
            }
        }
        check(inBounds, "noise2D over 0..8 grid stays within [-1.001, 1.001]");
        check(maxAbs > 0.05f, "noise2D field actually varies (max abs > 0.05)");
    }

    // --- 5. noise2D == noise3D(x, y, 0) --------------------------------------
    {
        PerlinNoise n(55);
        check(n.noise2D(0.5f, 0.5f) == n.noise3D(0.5f, 0.5f, 0.0f), "noise2D(0.5,0.5) == noise3D(0.5,0.5,0)");
        check(n.noise2D(1.3f, -2.7f) == n.noise3D(1.3f, -2.7f, 0.0f), "noise2D(1.3,-2.7) == noise3D(1.3,-2.7,0)");
        check(n.noise2D(10.1f, 3.9f) == n.noise3D(10.1f, 3.9f, 0.0f), "noise2D(10.1,3.9) == noise3D(10.1,3.9,0)");
    }

    // --- 6. fbm OCTAVES==1 == noise2D + fbm(4) BOUNDED ------------------------
    {
        PerlinNoise n(88);
        check(n.fbm2D(0.5f, 0.5f, 1) == n.noise2D(0.5f, 0.5f), "fbm2D(0.5,0.5,1) == noise2D(0.5,0.5)");
        check(n.fbm2D(1.3f, -2.7f, 1) == n.noise2D(1.3f, -2.7f), "fbm2D(1.3,-2.7,1) == noise2D(1.3,-2.7)");
        float f4 = n.fbm2D(0.5f, 0.5f, 4);
        check(std::isfinite(f4), "fbm2D(0.5,0.5,4) is finite");
        check(f4 >= -1.9f && f4 <= 1.9f, "fbm2D(0.5,0.5,4) within [-1.9, 1.9]");
    }

    // --- 7. fbm DETERMINISM --------------------------------------------------
    {
        PerlinNoise n(88);
        check(n.fbm2D(1.3f, -2.7f, 4) == n.fbm2D(1.3f, -2.7f, 4), "fbm2D(1.3,-2.7,4) deterministic across calls");
    }

    // Silence unused-function warning if approx() is not otherwise referenced.
    (void)approx;

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
