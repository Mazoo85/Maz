#pragma once

#include "maz/core/Fixed.hpp"

#include <cstdint>

// maz::math::FixedTrig — DETERMINISTIC fixed-point sine and cosine for core::Fixed (Q16.16), computed
// by an integer CORDIC (COordinate Rotation DIgital Computer) with no floats on the runtime path. Where
// std::sin/std::cos give subtly different bits on different CPUs/compilers/optimisation levels — silently
// desyncing lockstep multiplayer and replays — this rotates an integer vector by a table of precomputed
// arctangents using only shifts, adds, and one integer scale, so identical inputs give identical bits
// everywhere. Angles are in radians (Fixed). Accuracy is ~1e-4 (limited by 16 fractional bits), plenty
// for gameplay rotation. Godot has no fixed-point trig at all, so this is a genuine gap. Header-only.
namespace maz::math {

using core::Fixed;

namespace detail {

// pi, 2pi, pi/2 as Q16.16 raw values. kTwoPi/kHalfPi are kept consistent with kPi so that argument
// reduction (subtracting whole turns) stays exact in the fixed representation.
inline constexpr std::int32_t kFixPiRaw = 205887;               // round(pi * 65536)
inline constexpr std::int32_t kFixTwoPiRaw = 2 * kFixPiRaw;     // 411774
inline constexpr std::int32_t kFixHalfPiRaw = 102944;           // round((pi/2) * 65536)

// CORDIC rotation gain K = product 1/sqrt(1 + 2^-2i) over the iterations, as Q16.16 raw. Pre-loading
// x with K means the rotated vector already has unit length, so no post-scale is needed.
inline constexpr std::int64_t kCordicGainRaw = 39797;           // round(0.6072529350 * 65536)

// atan(2^-i) in radians, Q16.16 raw, for i = 0..15. This is the whole "cost" of CORDIC: a tiny table.
inline constexpr std::int32_t kCordicAtan[16] = {
    51472, // atan(1)
    30386, // atan(1/2)
    16055, // atan(1/4)
    8150,  // atan(1/8)
    4091,  // atan(1/16)
    2048,  // atan(1/32)
    1024,  // atan(1/64)
    512,   // atan(1/128)
    256,   // atan(1/256)
    128,   // atan(1/512)
    64,    // atan(1/1024)
    32,    // atan(1/2048)
    16,    // atan(1/4096)
    8,     // atan(1/8192)
    4,     // atan(1/16384)
    2,     // atan(1/32768)
};

} // namespace detail

// A cosine + sine pair, computed together in one CORDIC pass so they are always mutually consistent.
struct FixSinCos {
    Fixed cos;
    Fixed sin;
};

// Deterministic sin and cos of an angle in radians, in one pass (share the work; guaranteed consistent).
inline FixSinCos fixSinCos(Fixed angle) {
    std::int64_t z = angle.raw;

    // Reduce to [-pi, pi] by subtracting whole turns (exact in fixed representation).
    while (z > detail::kFixPiRaw) {
        z -= detail::kFixTwoPiRaw;
    }
    while (z < -detail::kFixPiRaw) {
        z += detail::kFixTwoPiRaw;
    }

    // Fold the outer quadrants into [-pi/2, pi/2] (CORDIC only converges there); cos flips sign when we do.
    bool negateCos = false;
    if (z > detail::kFixHalfPiRaw) {
        z = detail::kFixPiRaw - z;
        negateCos = true;
    } else if (z < -detail::kFixHalfPiRaw) {
        z = -detail::kFixPiRaw - z;
        negateCos = true;
    }

    // Rotate the vector (K, 0) by the target angle using only shifts and adds. Signed right shift is
    // arithmetic in C++20, which is exactly what CORDIC needs for the y>>i / x>>i micro-rotations.
    std::int64_t x = detail::kCordicGainRaw;
    std::int64_t y = 0;
    for (int i = 0; i < 16; ++i) {
        const std::int64_t dx = y >> i;
        const std::int64_t dy = x >> i;
        if (z >= 0) {
            x -= dx;
            y += dy;
            z -= detail::kCordicAtan[i];
        } else {
            x += dx;
            y -= dy;
            z += detail::kCordicAtan[i];
        }
    }

    FixSinCos out;
    out.cos = Fixed::fromRaw(static_cast<std::int32_t>(negateCos ? -x : x));
    out.sin = Fixed::fromRaw(static_cast<std::int32_t>(y));
    return out;
}

// Deterministic sine of an angle in radians (Fixed).
inline Fixed fixSin(Fixed angle) { return fixSinCos(angle).sin; }

// Deterministic cosine of an angle in radians (Fixed).
inline Fixed fixCos(Fixed angle) { return fixSinCos(angle).cos; }

// The circle constants as Fixed, for callers building angles deterministically.
inline Fixed fixPi() { return Fixed::fromRaw(detail::kFixPiRaw); }
inline Fixed fixTwoPi() { return Fixed::fromRaw(detail::kFixTwoPiRaw); }
inline Fixed fixHalfPi() { return Fixed::fromRaw(detail::kFixHalfPiRaw); }

} // namespace maz::math
