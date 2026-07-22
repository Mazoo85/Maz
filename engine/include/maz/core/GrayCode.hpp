#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

// maz::core Gray code + bit-change utilities. A Gray code (reflected binary) orders the integers so that
// each value differs from the previous by EXACTLY ONE bit — unlike ordinary binary, where e.g. 3->4 flips
// three bits at once. That single-bit-change property is what makes Gray codes glitch-free for rotary /
// position encoders (a reading caught mid-transition is off by at most one), and it gives a minimal-change
// enumeration order for combinations/subsets, dithering and LOD-transition sequences, and error-resilient
// counters. The paired `hammingDistance` counts how many bits differ between two values (popcount of XOR) —
// the same "how many bits changed" measure, also the standard metric for comparing perceptual image hashes
// (dHash/pHash) and bitmask diffs. None of these are in the standard library (unlike popcount / bit_ceil /
// countl_zero, which the engine already leans on). Header-only, std-only, deterministic; Godot ships none.
namespace maz::core {

// Binary -> reflected Gray code: g = n XOR (n >> 1). Consecutive n produce codes one bit apart.
template <typename T>
constexpr T grayEncode(T n) {
    static_assert(std::is_unsigned_v<T>, "grayEncode requires an unsigned integer type");
    return static_cast<T>(n ^ (n >> 1));
}

// Reflected Gray code -> binary (the inverse of grayEncode).
template <typename T>
constexpr T grayDecode(T g) {
    static_assert(std::is_unsigned_v<T>, "grayDecode requires an unsigned integer type");
    T b = g;
    for (T shift = g >> 1; shift != 0; shift = static_cast<T>(shift >> 1)) {
        b = static_cast<T>(b ^ shift);
    }
    return b;
}

// Number of differing bits between a and b (Hamming distance = popcount of the XOR).
template <typename T>
constexpr int hammingDistance(T a, T b) {
    static_assert(std::is_unsigned_v<T>, "hammingDistance requires an unsigned integer type");
    return std::popcount(static_cast<T>(a ^ b));
}

// The full 2^bits Gray-code sequence in order (bits clamped to [0, 20] to bound the allocation). Each entry
// differs from its neighbour by exactly one bit, and the sequence is a permutation of [0, 2^bits).
inline std::vector<std::uint32_t> graySequence(int bits) {
    if (bits < 0) {
        bits = 0;
    }
    if (bits > 20) {
        bits = 20;
    }
    const std::size_t count = static_cast<std::size_t>(1) << static_cast<unsigned>(bits);
    std::vector<std::uint32_t> seq;
    seq.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        seq.push_back(grayEncode(i));
    }
    return seq;
}

} // namespace maz::core
