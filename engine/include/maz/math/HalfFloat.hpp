#pragma once

#include <cstdint>
#include <cstring>

// maz::math IEEE 754 half-precision (float16) conversions — the 16-bit floating-point format Godot exposes
// as `Math::half_to_float` / `Math::make_half_float` and leans on for HDR image storage, glTF quantized
// vertex accessors, and GPU vertex/attribute compression (half the bandwidth of float32). Maz had no way to
// pack or unpack halves, so any code touching a .hdr/.exr-style buffer, a KTX2 float texture, or a
// half-packed vertex stream had to hand-roll the bit twiddling. `floatToHalf` rounds to nearest, ties to
// even (the IEEE default and what GPUs do), saturating out-of-range magnitudes to infinity and flushing the
// tiniest values through the subnormal range to zero; `halfToFloat` is exact. Header-only, pure integer bit
// work — unit-tested headlessly against the reference half encoding produced by Python's `struct` module.
namespace maz::math {

// Expand a 16-bit half to a 32-bit float. Exact for every one of the 65 536 half bit patterns.
inline float halfToFloat(std::uint16_t h) {
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    std::uint32_t exp = (h >> 10) & 0x1fu;
    std::uint32_t mant = h & 0x3ffu;
    std::uint32_t bits;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign; // signed zero
        } else {
            // Subnormal half: renormalize into a normal float.
            exp = 1;
            while ((mant & 0x400u) == 0) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x3ffu;
            bits = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
        }
    } else if (exp == 0x1fu) {
        bits = sign | 0x7f800000u | (mant << 13); // infinity / NaN
    } else {
        bits = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// Pack a 32-bit float into a 16-bit half, rounding to nearest with ties to even. Magnitudes above the half
// range become infinity; magnitudes below the smallest subnormal become signed zero; NaN stays NaN.
inline std::uint16_t floatToHalf(float f) {
    std::uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    const std::uint16_t sign = static_cast<std::uint16_t>((x >> 16) & 0x8000u);
    const std::uint32_t rawExp = (x >> 23) & 0xffu;
    const std::uint32_t mant = x & 0x7fffffu;

    if (rawExp == 0xffu) {
        // Infinity or NaN. Preserve NaN-ness (any non-zero mantissa -> a quiet NaN).
        return static_cast<std::uint16_t>(sign | 0x7c00u | (mant ? 0x200u : 0u));
    }

    const int exp = static_cast<int>(rawExp) - 127 + 15; // rebiased half exponent
    if (exp >= 0x1f) {
        return static_cast<std::uint16_t>(sign | 0x7c00u); // overflow -> infinity
    }
    if (exp <= 0) {
        if (exp < -10) {
            return sign; // too small even for a subnormal -> signed zero
        }
        // Subnormal result: restore the implicit leading 1, then shift with round-to-nearest-even.
        const std::uint32_t m = mant | 0x800000u;
        const int shift = 14 - exp; // 14..24
        std::uint32_t half = m >> shift;
        const std::uint32_t rem = m & ((1u << shift) - 1u);
        const std::uint32_t halfway = 1u << (shift - 1);
        if (rem > halfway || (rem == halfway && (half & 1u))) ++half;
        return static_cast<std::uint16_t>(sign | half);
    }
    // Normal result. Round the 23-bit mantissa down to 10 bits, ties to even; a carry propagates cleanly
    // into the exponent field (and, at the top, into infinity).
    std::uint16_t out = static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exp) << 10) | (mant >> 13));
    const std::uint32_t rem = mant & 0x1fffu;
    if (rem > 0x1000u || (rem == 0x1000u && ((mant >> 13) & 1u))) ++out;
    return out;
}

} // namespace maz::math
