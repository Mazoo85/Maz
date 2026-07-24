#pragma once

#include <cstdint>

// maz::math Morton (Z-order) codes — interleave the bits of 2D or 3D integer coordinates into a single
// number that snakes through space along a Z-shaped curve, so points that are near each other in space get
// numbers that are (mostly) near each other too. This is the workhorse key for LINEAR quadtrees/octrees
// (store a sparse tree as a sorted list of Morton codes), cache-coherent grid/texture traversal and GPU
// swizzle layouts, spatial hashing, and sorting particles/entities so neighbours sit close in memory. Encode
// packs coordinates into the code; decode unpacks them back exactly. 2D takes two 16-bit coords into a 32-bit
// code; 3D takes three 10-bit coords into a 30-bit code. Godot exposes no Morton helpers. Header-only,
// std-only, deterministic.
namespace maz::math {

namespace morton_detail {
// Spread the low 16 bits of x so each occupies an even bit position (insert a 0 between each).
inline std::uint32_t part1By1(std::uint32_t x) {
    x &= 0x0000FFFFu;
    x = (x | (x << 8)) & 0x00FF00FFu;
    x = (x | (x << 4)) & 0x0F0F0F0Fu;
    x = (x | (x << 2)) & 0x33333333u;
    x = (x | (x << 1)) & 0x55555555u;
    return x;
}
// Inverse of part1By1: gather every even bit back into the low 16 bits.
inline std::uint32_t compact1By1(std::uint32_t x) {
    x &= 0x55555555u;
    x = (x | (x >> 1)) & 0x33333333u;
    x = (x | (x >> 2)) & 0x0F0F0F0Fu;
    x = (x | (x >> 4)) & 0x00FF00FFu;
    x = (x | (x >> 8)) & 0x0000FFFFu;
    return x;
}
// Spread the low 10 bits of x so each occupies every third bit position (insert two 0s between each).
inline std::uint32_t part1By2(std::uint32_t x) {
    x &= 0x000003FFu;
    x = (x | (x << 16)) & 0xFF0000FFu;
    x = (x | (x << 8)) & 0x0300F00Fu;
    x = (x | (x << 4)) & 0x030C30C3u;
    x = (x | (x << 2)) & 0x09249249u;
    return x;
}
// Inverse of part1By2.
inline std::uint32_t compact1By2(std::uint32_t x) {
    x &= 0x09249249u;
    x = (x | (x >> 2)) & 0x030C30C3u;
    x = (x | (x >> 4)) & 0x0300F00Fu;
    x = (x | (x >> 8)) & 0xFF0000FFu;
    x = (x | (x >> 16)) & 0x000003FFu;
    return x;
}
} // namespace morton_detail

// Interleave two 16-bit coordinates into a 32-bit Morton (Z-order) code (x in the even bits, y in the odd).
inline std::uint32_t mortonEncode2D(std::uint16_t x, std::uint16_t y) {
    return morton_detail::part1By1(x) | (morton_detail::part1By1(y) << 1);
}

// Recover the two coordinates from a 2D Morton code (inverse of mortonEncode2D).
inline void mortonDecode2D(std::uint32_t code, std::uint16_t& x, std::uint16_t& y) {
    x = static_cast<std::uint16_t>(morton_detail::compact1By1(code));
    y = static_cast<std::uint16_t>(morton_detail::compact1By1(code >> 1));
}

// Interleave three 10-bit coordinates into a 30-bit Morton (Z-order) code.
inline std::uint32_t mortonEncode3D(std::uint16_t x, std::uint16_t y, std::uint16_t z) {
    return morton_detail::part1By2(x) | (morton_detail::part1By2(y) << 1) | (morton_detail::part1By2(z) << 2);
}

// Recover the three coordinates from a 3D Morton code (inverse of mortonEncode3D).
inline void mortonDecode3D(std::uint32_t code, std::uint16_t& x, std::uint16_t& y, std::uint16_t& z) {
    x = static_cast<std::uint16_t>(morton_detail::compact1By2(code));
    y = static_cast<std::uint16_t>(morton_detail::compact1By2(code >> 1));
    z = static_cast<std::uint16_t>(morton_detail::compact1By2(code >> 2));
}

} // namespace maz::math
