#pragma once

#include <cstdint>
#include <bit>  // std::bit_cast (C++20)

#include "maz/core/Assert.hpp"

// IEEE 754 binary16 (half-float) <-> binary32 conversion for GPU vertex/texture
// compression and HDR storage in maz::math. floatToHalf packs a float into a
// 16-bit half with round-to-nearest-even, overflow to +/- Inf, subnormal-half
// handling, and NaN/Inf passthrough; halfToFloat unpacks a half back to a float
// exactly — every half is representable as a float, so the decode is lossless.
// Uses std::bit_cast to reinterpret the bit patterns (no aliasing UB, constexpr-
// friendly). The Godot half-float analog. This is not a full `half` arithmetic
// type — convert to float to compute, then convert back to store.

namespace maz::math {

// binary32 -> binary16. Round-to-nearest-even; overflow saturates to +/- Inf,
// subnormal-magnitude inputs encode as subnormal halves (or +/- 0 when too
// small), and NaN/Inf pass through as a quiet NaN / signed Inf.
inline std::uint16_t floatToHalf(float value) {
    const std::uint32_t x = std::bit_cast<std::uint32_t>(value);
    const std::uint32_t sign = (x >> 16) & 0x8000u;
    std::int32_t exp = static_cast<std::int32_t>((x >> 23) & 0xFFu) - 127 + 15;  // rebias 127 -> 15
    const std::uint32_t mantissa = x & 0x7FFFFFu;

    // NaN / Inf (f32 exponent all ones).
    if (((x >> 23) & 0xFFu) == 0xFFu) {
        if (mantissa != 0u) {
            return static_cast<std::uint16_t>(sign | 0x7E00u);  // NaN (quiet, non-zero mantissa)
        }
        return static_cast<std::uint16_t>(sign | 0x7C00u);  // +/- Inf
    }

    if (exp >= 0x1F) {
        // Overflow -> Inf.
        return static_cast<std::uint16_t>(sign | 0x7C00u);
    }
    if (exp <= 0) {
        // Subnormal half or underflow to zero.
        if (exp < -10) {
            return static_cast<std::uint16_t>(sign);  // too small -> +/- 0
        }
        // Add the implicit leading 1, then shift into subnormal position with rounding.
        const std::uint32_t m = mantissa | 0x800000u;                   // 24-bit significand
        const std::int32_t shift = 14 - exp;                            // exp in [-10, 0] -> shift in [14, 24]
        const std::uint32_t halfMant = m >> shift;
        // Round-to-nearest-even on the shifted-out bits.
        const std::uint32_t roundBits = m & ((1u << shift) - 1u);
        const std::uint32_t halfway = 1u << (shift - 1);
        std::uint32_t result = halfMant;
        if (roundBits > halfway || (roundBits == halfway && (halfMant & 1u))) {
            result += 1u;  // may carry into the exponent field (subnormal -> smallest normal), which is correct
        }
        return static_cast<std::uint16_t>(sign | result);
    }
    // Normalized.
    const std::uint32_t halfExp = static_cast<std::uint32_t>(exp) << 10;
    const std::uint32_t halfMant = mantissa >> 13;
    const std::uint32_t roundBits = mantissa & 0x1FFFu;  // low 13 bits dropped
    std::uint32_t result = halfExp | halfMant;
    // Round-to-nearest-even.
    const std::uint32_t halfway = 0x1000u;  // 1 << 12
    if (roundBits > halfway || (roundBits == halfway && (halfMant & 1u))) {
        result += 1u;  // carry into exponent is fine; overflow to 0x7C00 is Inf (correct on overflow rounding)
    }
    return static_cast<std::uint16_t>(sign | result);
}

// binary16 -> binary32. Exact: every half maps to a representable float, so no
// rounding occurs. Subnormal halves normalize into the float exponent range,
// and half Inf/NaN map to float Inf/NaN.
inline float halfToFloat(std::uint16_t h) {
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    const std::uint32_t exp = (h >> 10) & 0x1Fu;
    const std::uint32_t mant = h & 0x3FFu;
    if (exp == 0u) {
        if (mant == 0u) {
            return std::bit_cast<float>(sign);  // +/- 0
        }
        // Subnormal half -> normalized float: shift mantissa until the implicit 1 is in place.
        std::uint32_t m = mant;
        std::int32_t e = -1;
        do {
            m <<= 1;
            ++e;
        } while ((m & 0x400u) == 0u);  // until bit 10 set
        m &= 0x3FFu;                   // drop the implicit 1
        // Each left shift halves the value, so the float exponent falls with e.
        const std::uint32_t f32exp = static_cast<std::uint32_t>(127 - 15 - e);
        return std::bit_cast<float>(sign | (f32exp << 23) | (m << 13));
    }
    if (exp == 0x1Fu) {
        // Inf / NaN.
        return std::bit_cast<float>(sign | 0x7F800000u | (mant << 13));
    }
    // Normalized.
    const std::uint32_t f32exp = exp - 15u + 127u;
    return std::bit_cast<float>(sign | (f32exp << 23) | (mant << 13));
}

} // namespace maz::math
