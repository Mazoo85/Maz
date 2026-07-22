#pragma once

#include <cmath>
#include <cstdint>

// maz::net bounded float quantization — shrink a float to N bits for network snapshots.
//
// Sending full 32-bit floats for every position, angle, and health value burns bandwidth. Almost all of
// them live in a KNOWN range (a level is ±1000 units; health is 0..100; an angle is one turn), so you can
// map that range onto a small integer of `bits` bits, send the integer, and reconstruct the float on the
// other side to within one quantization step. This is the core trick behind compact netcode snapshots and
// delta encoding (pairs with the engine's BitStream/Snapshot). `quantizeFloat`/`dequantizeFloat` handle an
// arbitrary [min,max] range; `quantizeAngle`/`dequantizeAngle` treat an angle as PERIODIC so -pi and +pi
// map to the same code and there is no seam. Unlike PackNorm (fixed [0,1]/[-1,1] at 8/16 bits for GPU
// vertex attributes), this is any range at any bit width. Pure math, header-only, deterministic —
// unit-tested for exact endpoints, round-trip within half a step, clamping, and finer error at more bits.
namespace maz::net {

namespace detail {
inline float fqClamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }
inline std::uint32_t fqMaxInt(int bits) {
    if (bits < 1) bits = 1;
    if (bits > 31) bits = 31;
    return (1u << static_cast<unsigned>(bits)) - 1u;
}
} // namespace detail

// Map `value` in [min,max] to an integer in [0, 2^bits - 1] (clamps out-of-range values).
inline std::uint32_t quantizeFloat(float value, float min, float max, int bits) {
    const std::uint32_t maxInt = detail::fqMaxInt(bits);
    if (max <= min) return 0;
    const float t = detail::fqClamp01((value - min) / (max - min));
    return static_cast<std::uint32_t>(std::lround(t * static_cast<float>(maxInt)));
}

// Reconstruct the float from a quantized code.
inline float dequantizeFloat(std::uint32_t q, float min, float max, int bits) {
    const std::uint32_t maxInt = detail::fqMaxInt(bits);
    if (maxInt == 0) return min;
    return min + (static_cast<float>(q) / static_cast<float>(maxInt)) * (max - min);
}

// Periodic angle quantization: any angle (radians) -> [0, 2^bits). -pi and +pi share a code; no seam.
inline std::uint32_t quantizeAngle(float radians, int bits) {
    if (bits < 1) bits = 1;
    if (bits > 31) bits = 31;
    const float twoPi = 6.28318530717958648f;
    float t = radians / twoPi;
    t = t - std::floor(t); // wrap into [0,1)
    const std::uint32_t n = 1u << static_cast<unsigned>(bits);
    const std::uint32_t q = static_cast<std::uint32_t>(std::lround(t * static_cast<float>(n)));
    return q & (n - 1u); // mod n (handles the t->1 rounding case)
}

// Reconstruct an angle in [0, 2*pi) from a quantized code.
inline float dequantizeAngle(std::uint32_t q, int bits) {
    if (bits < 1) bits = 1;
    if (bits > 31) bits = 31;
    const float twoPi = 6.28318530717958648f;
    const std::uint32_t n = 1u << static_cast<unsigned>(bits);
    return (static_cast<float>(q & (n - 1u)) / static_cast<float>(n)) * twoPi;
}

} // namespace maz::net
