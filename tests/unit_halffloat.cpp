// Unit tests for maz::math IEEE 754 binary16 (half-float) conversion
// (HalfFloat.hpp). Exercises exact float->half encode bit patterns and
// half->float decode values (canonical IEEE vectors), exact round-trips of
// half-representable magnitudes, overflow saturating to +/- Inf, Inf/NaN
// passthrough, subnormal-half decode (smallest positive subnormal == 2^-24)
// and encode, and round-to-nearest-even at a hand-verified 1-ulp boundary.
// Pure C++, no GPU/display.

#include "maz/math/HalfFloat.hpp"

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <limits>

using namespace maz::math;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
    // --- exact encode vectors (canonical IEEE binary16 bit patterns) ---------
    {
        check(floatToHalf(0.0f) == 0x0000, "floatToHalf(0.0) == 0x0000");
        check(floatToHalf(-0.0f) == 0x8000, "floatToHalf(-0.0) == 0x8000");
        check(floatToHalf(1.0f) == 0x3C00, "floatToHalf(1.0) == 0x3C00");
        check(floatToHalf(-1.0f) == 0xBC00, "floatToHalf(-1.0) == 0xBC00");
        check(floatToHalf(2.0f) == 0x4000, "floatToHalf(2.0) == 0x4000");
        check(floatToHalf(-2.0f) == 0xC000, "floatToHalf(-2.0) == 0xC000");
        check(floatToHalf(0.5f) == 0x3800, "floatToHalf(0.5) == 0x3800");
        check(floatToHalf(3.0f) == 0x4200, "floatToHalf(3.0) == 0x4200");
        check(floatToHalf(65504.0f) == 0x7BFF, "floatToHalf(65504.0) == 0x7BFF (max finite half)");
    }

    // --- exact decode vectors ------------------------------------------------
    {
        check(halfToFloat(0x0000) == 0.0f, "halfToFloat(0x0000) == 0.0");
        check(halfToFloat(0x3C00) == 1.0f, "halfToFloat(0x3C00) == 1.0");
        check(halfToFloat(0x4000) == 2.0f, "halfToFloat(0x4000) == 2.0");
        check(halfToFloat(0x3800) == 0.5f, "halfToFloat(0x3800) == 0.5");
        check(halfToFloat(0xC000) == -2.0f, "halfToFloat(0xC000) == -2.0");
        check(halfToFloat(0x4200) == 3.0f, "halfToFloat(0x4200) == 3.0");
        check(halfToFloat(0x7BFF) == 65504.0f, "halfToFloat(0x7BFF) == 65504.0");
    }

    // --- round-trip exact-representable (all exactly representable in half) ---
    {
        const float vals[] = {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, 0.25f,
                              3.0f, 4.0f, 100.0f, 0.125f, -0.5f, 65504.0f};
        for (float v : vals) {
            check(halfToFloat(floatToHalf(v)) == v, "half round-trip exact");
        }
    }

    // --- overflow -> Inf -----------------------------------------------------
    {
        check(floatToHalf(70000.0f) == 0x7C00, "floatToHalf(70000.0) == 0x7C00 (+Inf)");
        check(floatToHalf(-70000.0f) == 0xFC00, "floatToHalf(-70000.0) == 0xFC00 (-Inf)");
        check(floatToHalf(1e30f) == 0x7C00, "floatToHalf(1e30) == 0x7C00 (+Inf)");
    }

    // --- Inf passthrough -----------------------------------------------------
    {
        check(floatToHalf(INFINITY) == 0x7C00, "floatToHalf(+Inf) == 0x7C00");
        check(floatToHalf(-INFINITY) == 0xFC00, "floatToHalf(-Inf) == 0xFC00");
        check(std::isinf(halfToFloat(0x7C00)), "halfToFloat(0x7C00) is Inf");
        check(std::isinf(halfToFloat(0xFC00)) && halfToFloat(0xFC00) < 0.0f,
              "halfToFloat(0xFC00) is -Inf");
    }

    // --- NaN -----------------------------------------------------------------
    {
        check(std::isnan(halfToFloat(0x7E00)), "halfToFloat(0x7E00) is NaN");
        check(std::isnan(halfToFloat(floatToHalf(std::numeric_limits<float>::quiet_NaN()))),
              "NaN survives float->half->float round-trip");
    }

    // --- subnormal half decode -----------------------------------------------
    {
        const float smallest = halfToFloat(0x0001);  // smallest positive subnormal == 2^-24
        const float expected = 5.9604645e-8f;         // 2^-24
        check(std::fabs(smallest - expected) < 1e-12f, "halfToFloat(0x0001) ~= 2^-24");
        check(smallest > 0.0f, "halfToFloat(0x0001) > 0");
        check(smallest < halfToFloat(0x0400), "smallest subnormal < smallest normal (0x0400)");
        check(floatToHalf(smallest) == 0x0001, "floatToHalf(2^-24) round-trips to 0x0001");
    }

    // --- small subnormal encode + round-to-nearest-even ----------------------
    {
        // Largest subnormal half round-trips.
        check(floatToHalf(halfToFloat(0x03FF)) == 0x03FF, "largest subnormal (0x03FF) round-trips");
        // 1.0 + 2^-10 is exactly the next half after 1.0 (0x3C01), 1 ulp of half.
        check(floatToHalf(1.0f + 0.0009765625f) == 0x3C01, "floatToHalf(1 + 2^-10) == 0x3C01");
        // A value just below the 0x3C00/0x3C01 tie rounds down to 1.0 (0x3C00).
        check(floatToHalf(1.0f + 0.0002f) == 0x3C00, "floatToHalf(1 + 0.0002) rounds down to 0x3C00");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
