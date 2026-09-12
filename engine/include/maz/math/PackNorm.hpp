#pragma once

#include "maz/math/MathFuncs.hpp" // clampf

#include <cmath>
#include <cstdint>

// maz::math fixed-point normalized packing (UNORM / SNORM) — the quantization layer of GPU vertex and
// attribute compression, converting a normalized float to an 8- or 16-bit integer and back. This is the
// piece that actually shrinks the data: paired with `octahedronEncode` (VectorOps) it stores a surface
// normal in two 16-bit integers instead of three 32-bit floats, exactly how Godot's compressed mesh format
// keeps normals/tangents, and it is the standard Vulkan/OpenGL vertex-attribute encoding. UNORM maps [0,1]
// to [0, 2^bits-1]; SNORM maps [-1,1] to [-(2^(bits-1)-1), +(2^(bits-1)-1)] (the Khronos symmetric form, so
// -1 and +1 are exact and 0 lands on 0). Rounding is round-half-away-from-zero (the GL `round()` rule).
// Pure integer/float math — no GPU — so it unit-tests headlessly by round-trip and exact endpoints.
//
// Scope note (honest): 8/16-bit UNORM/SNORM scalar packing. The caller composes these per channel (e.g. the
// two components of an octahedral normal); vector overloads and the packed 10/10/10/2 formats are
// documented follow-ups.
namespace maz::math {

// UNORM: float in [0,1] -> unsigned integer in [0, MAX].
inline std::uint8_t packUnorm8(float f) {
    return static_cast<std::uint8_t>(std::lround(clampf(f, 0.0f, 1.0f) * 255.0f));
}
inline float unpackUnorm8(std::uint8_t u) { return static_cast<float>(u) / 255.0f; }

inline std::uint16_t packUnorm16(float f) {
    return static_cast<std::uint16_t>(std::lround(clampf(f, 0.0f, 1.0f) * 65535.0f));
}
inline float unpackUnorm16(std::uint16_t u) { return static_cast<float>(u) / 65535.0f; }

// SNORM: float in [-1,1] -> signed integer in [-MAX, MAX]. Unpacking clamps at -1 so the extreme negative
// code (if a caller supplies -MAX-1) still yields exactly -1.
inline std::int8_t packSnorm8(float f) {
    return static_cast<std::int8_t>(std::lround(clampf(f, -1.0f, 1.0f) * 127.0f));
}
inline float unpackSnorm8(std::int8_t i) { return std::fmax(static_cast<float>(i) / 127.0f, -1.0f); }

inline std::int16_t packSnorm16(float f) {
    return static_cast<std::int16_t>(std::lround(clampf(f, -1.0f, 1.0f) * 32767.0f));
}
inline float unpackSnorm16(std::int16_t i) { return std::fmax(static_cast<float>(i) / 32767.0f, -1.0f); }

} // namespace maz::math
