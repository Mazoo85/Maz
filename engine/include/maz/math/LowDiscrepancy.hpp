#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cstdint>

// maz::math low-discrepancy (quasi-random) sequences — Halton, Hammersley, and the van der Corput
// radical inverse they are built on. Unlike a pseudo-random generator (which clumps and leaves gaps),
// these sequences fill the unit interval/square as EVENLY as possible for any prefix count, which is
// exactly what you want for: temporal anti-aliasing sub-pixel jitter (a different well-spread offset
// each frame), progressive/quasi-Monte-Carlo sampling (soft shadows, AO, IBL importance sampling that
// converges faster than white noise), and even scatter placement. The radical inverse reflects an
// integer's digits (in some base) about the decimal point; Halton pairs two coprime-base radical
// inverses; Hammersley uses the sample index directly for one axis. Pure integer/float math, no state,
// no allocation — deterministic and exactly unit-testable against the sequence's known values.
namespace maz::math {

// Van der Corput radical inverse of `i` in the given base: reflect the base-`b` digits of i about the
// radix point, giving a value in [0,1). base 2 gives 1/2, 1/4, 3/4, 1/8, 5/8, ... for i = 1,2,3,4,5,...
inline float radicalInverse(uint32_t base, uint32_t i) {
    if (base < 2) {
        return 0.0f;
    }
    const float inv = 1.0f / static_cast<float>(base);
    float invPow = inv;
    float result = 0.0f;
    while (i > 0) {
        const uint32_t digit = i % base;
        result += static_cast<float>(digit) * invPow;
        i /= base;
        invPow *= inv;
    }
    return result;
}

// The base-2 van der Corput sequence (the most common 1D low-discrepancy sequence).
inline float vanDerCorput2(uint32_t i) { return radicalInverse(2, i); }

// A 2D Halton point: independent radical inverses in two (ideally coprime) bases. Defaults to the
// classic (2,3) Halton sequence.
inline vec2 halton2D(uint32_t i, uint32_t baseX = 2, uint32_t baseY = 3) {
    return vec2(radicalInverse(baseX, i), radicalInverse(baseY, i));
}

// A 2D Hammersley point for a set of `count` samples: x = i/count (needs the total up front), y =
// base-2 radical inverse. Slightly better spread than Halton when the sample count is known.
inline vec2 hammersley2D(uint32_t i, uint32_t count) {
    const float x = count > 0 ? static_cast<float>(i) / static_cast<float>(count) : 0.0f;
    return vec2(x, radicalInverse(2, i));
}

} // namespace maz::math
