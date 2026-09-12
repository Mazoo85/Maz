#pragma once

#include <cstdint>
#include <utility>

// maz::core — space-filling curve encoders: Morton (Z-order) and Hilbert. These map multi-dimensional
// integer grid coordinates to a single scalar index (and back) such that points close on the line tend
// to be close in space. They are the standard tool for cache-coherent grid traversal, spatial hash keys,
// quadtree/octree node ordering, texture swizzling, and locality-preserving sorts. Morton codes are the
// cheap workhorse (pure bit interleaving); the Hilbert curve is costlier but has strictly better locality
// — consecutive indices are ALWAYS grid neighbours (Manhattan distance exactly 1), which Morton's
// diagonal jumps do not guarantee. Godot exposes no built-in space-filling curve, so this is a
// beyond-Godot utility. All functions are exact bijections over their coordinate range. Header-only,
// std-only.
namespace maz::core {

// --- Morton / Z-order ------------------------------------------------------------------------------
// 2D: interleave up to 32 bits of x and y into a 64-bit code. 3D: interleave up to 21 bits each.

namespace detail {

// Spread the low 32 bits of x so each occupies an even bit position (bit i -> bit 2i).
inline uint64_t part1By1(uint32_t x) {
    uint64_t v = x;
    v = (v | (v << 16)) & 0x0000FFFF0000FFFFULL;
    v = (v | (v << 8)) & 0x00FF00FF00FF00FFULL;
    v = (v | (v << 4)) & 0x0F0F0F0F0F0F0F0FULL;
    v = (v | (v << 2)) & 0x3333333333333333ULL;
    v = (v | (v << 1)) & 0x5555555555555555ULL;
    return v;
}

// Inverse of part1By1: gather the even bits of v back into the low 32 bits.
inline uint32_t compact1By1(uint64_t v) {
    v &= 0x5555555555555555ULL;
    v = (v | (v >> 1)) & 0x3333333333333333ULL;
    v = (v | (v >> 2)) & 0x0F0F0F0F0F0F0F0FULL;
    v = (v | (v >> 4)) & 0x00FF00FF00FF00FFULL;
    v = (v | (v >> 8)) & 0x0000FFFF0000FFFFULL;
    v = (v | (v >> 16)) & 0x00000000FFFFFFFFULL;
    return static_cast<uint32_t>(v);
}

// Spread the low 21 bits of x so each occupies every third bit position (bit i -> bit 3i).
inline uint64_t part1By2(uint32_t x) {
    uint64_t v = x & 0x1FFFFFULL;
    v = (v | (v << 32)) & 0x1F00000000FFFFULL;
    v = (v | (v << 16)) & 0x1F0000FF0000FFULL;
    v = (v | (v << 8)) & 0x100F00F00F00F00FULL;
    v = (v | (v << 4)) & 0x10C30C30C30C30C3ULL;
    v = (v | (v << 2)) & 0x1249249249249249ULL;
    return v;
}

// Inverse of part1By2: gather every third bit of v back into the low 21 bits.
inline uint32_t compact1By2(uint64_t v) {
    v &= 0x1249249249249249ULL;
    v = (v | (v >> 2)) & 0x10C30C30C30C30C3ULL;
    v = (v | (v >> 4)) & 0x100F00F00F00F00FULL;
    v = (v | (v >> 8)) & 0x1F0000FF0000FFULL;
    v = (v | (v >> 16)) & 0x1F00000000FFFFULL;
    v = (v | (v >> 32)) & 0x1FFFFFULL;
    return static_cast<uint32_t>(v);
}

} // namespace detail

// Interleave (x, y) into a Z-order code. x contributes the even bits, y the odd bits.
inline uint64_t mortonEncode2(uint32_t x, uint32_t y) {
    return detail::part1By1(x) | (detail::part1By1(y) << 1);
}

// Recover (x, y) from a Z-order code produced by mortonEncode2.
inline std::pair<uint32_t, uint32_t> mortonDecode2(uint64_t code) {
    return {detail::compact1By1(code), detail::compact1By1(code >> 1)};
}

// Interleave (x, y, z) (each up to 21 bits) into a Z-order code.
inline uint64_t mortonEncode3(uint32_t x, uint32_t y, uint32_t z) {
    return detail::part1By2(x) | (detail::part1By2(y) << 1) | (detail::part1By2(z) << 2);
}

// Recover (x, y, z) from a 3D Z-order code produced by mortonEncode3.
inline void mortonDecode3(uint64_t code, uint32_t& x, uint32_t& y, uint32_t& z) {
    x = detail::compact1By2(code);
    y = detail::compact1By2(code >> 1);
    z = detail::compact1By2(code >> 2);
}

// --- Hilbert curve (2D) ----------------------------------------------------------------------------
// `n` is the grid side length and MUST be a power of two. The curve visits every cell of the n x n grid
// exactly once with indices in [0, n*n); consecutive indices are always orthogonal grid neighbours.

namespace detail {

// Rotate/flip a quadrant so the recursion stays consistent (Wikipedia "Hilbert curve" reference form).
inline void hilbertRot(uint32_t n, uint32_t& x, uint32_t& y, uint32_t rx, uint32_t ry) {
    if (ry == 0) {
        if (rx == 1) {
            x = n - 1 - x;
            y = n - 1 - y;
        }
        const uint32_t t = x;
        x = y;
        y = t;
    }
}

} // namespace detail

// Map grid coordinates (x, y) to their Hilbert-curve distance d in [0, n*n).
inline uint64_t hilbertXY2D(uint32_t n, uint32_t x, uint32_t y) {
    uint64_t d = 0;
    for (uint32_t s = n / 2; s > 0; s /= 2) {
        const uint32_t rx = (x & s) > 0 ? 1u : 0u;
        const uint32_t ry = (y & s) > 0 ? 1u : 0u;
        d += static_cast<uint64_t>(s) * static_cast<uint64_t>(s) * ((3u * rx) ^ ry);
        detail::hilbertRot(n, x, y, rx, ry);
    }
    return d;
}

// Map a Hilbert-curve distance d back to grid coordinates (x, y).
inline void hilbertD2XY(uint32_t n, uint64_t d, uint32_t& x, uint32_t& y) {
    x = 0;
    y = 0;
    uint64_t t = d;
    for (uint32_t s = 1; s < n; s *= 2) {
        const uint32_t rx = 1u & static_cast<uint32_t>(t / 2);
        const uint32_t ry = 1u & static_cast<uint32_t>(t ^ rx);
        detail::hilbertRot(s, x, y, rx, ry);
        x += s * rx;
        y += s * ry;
        t /= 4;
    }
}

} // namespace maz::core
