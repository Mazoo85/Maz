#pragma once

#include <cstdint>
#include <cstddef>
#include <bit>  // std::endian (C++20)

#include "maz/core/Assert.hpp"

// Byte-order helpers for portable binary I/O in maz::core. Provides byteSwap16/
// 32/64 (each its own inverse — byteSwap(byteSwap(x)) == x); explicit-endianness
// pack/unpack (writeU32LE/readU32BE/...) that produce and consume a FIXED little-
// or big-endian byte layout regardless of host endianness (so a file written on
// one platform reads correctly on another); and host<->little/big scalar
// conversions via std::endian::native. Complements iter16 ByteWriter/ByteReader.
// Assumes a pure little- or big-endian host (mixed-endian unsupported). Operates
// on unsigned integers; for signed/float values, std::bit_cast to the unsigned
// width of the same size first.

namespace maz::core {

// --- BYTE SWAP (manual — std::byteswap is C++23, not available) -----------------

// Reverse the two bytes of a 16-bit value. Its own inverse.
inline constexpr std::uint16_t byteSwap16(std::uint16_t v) {
    return static_cast<std::uint16_t>((v << 8) | (v >> 8));
}

// Reverse the four bytes of a 32-bit value. Its own inverse.
inline constexpr std::uint32_t byteSwap32(std::uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

// Reverse the eight bytes of a 64-bit value. Its own inverse.
inline constexpr std::uint64_t byteSwap64(std::uint64_t v) {
    return ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) |
           ((v & 0x0000000000FF0000ull) << 24) | ((v & 0x00000000FF000000ull) << 8)  |
           ((v & 0x000000FF00000000ull) >> 8)  | ((v & 0x0000FF0000000000ull) >> 24) |
           ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
}

// --- EXPLICIT-ENDIANNESS PACK (write) — HOST-INDEPENDENT ------------------------
// Each writeUNXX writes exactly N/8 bytes into dst[0..N/8-1] in the named byte
// order; the caller ensures dst has room for that many bytes.

// Write 2 bytes, least-significant first.
inline void writeU16LE(std::uint8_t* dst, std::uint16_t v) {
    dst[0] = static_cast<std::uint8_t>(v & 0xFFu);
    dst[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

// Write 2 bytes, most-significant first.
inline void writeU16BE(std::uint8_t* dst, std::uint16_t v) {
    dst[0] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    dst[1] = static_cast<std::uint8_t>(v & 0xFFu);
}

// Write 4 bytes, least-significant first.
inline void writeU32LE(std::uint8_t* dst, std::uint32_t v) {
    dst[0] = static_cast<std::uint8_t>(v & 0xFFu);
    dst[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    dst[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    dst[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

// Write 4 bytes, most-significant first.
inline void writeU32BE(std::uint8_t* dst, std::uint32_t v) {
    dst[0] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
    dst[1] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    dst[2] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    dst[3] = static_cast<std::uint8_t>(v & 0xFFu);
}

// Write 8 bytes, least-significant first.
inline void writeU64LE(std::uint8_t* dst, std::uint64_t v) {
    for (std::size_t i = 0; i < 8; ++i) {
        dst[i] = static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu);
    }
}

// Write 8 bytes, most-significant first.
inline void writeU64BE(std::uint8_t* dst, std::uint64_t v) {
    for (std::size_t i = 0; i < 8; ++i) {
        dst[i] = static_cast<std::uint8_t>((v >> (8u * (7 - i))) & 0xFFu);
    }
}

// --- EXPLICIT-ENDIANNESS UNPACK (read) — HOST-INDEPENDENT -----------------------
// Each readUNXX reads exactly N/8 bytes from src[0..N/8-1] in the named byte
// order and is the exact inverse of the matching write for that endianness
// (readU32LE(after writeU32LE(x)) == x). Bytes are widened to the target width
// BEFORE shifting so the shift never overflows int.

// Read 2 bytes, least-significant first.
inline std::uint16_t readU16LE(const std::uint8_t* src) {
    return static_cast<std::uint16_t>(src[0] | (src[1] << 8));
}

// Read 2 bytes, most-significant first.
inline std::uint16_t readU16BE(const std::uint8_t* src) {
    return static_cast<std::uint16_t>((src[0] << 8) | src[1]);
}

// Read 4 bytes, least-significant first.
inline std::uint32_t readU32LE(const std::uint8_t* src) {
    return static_cast<std::uint32_t>(src[0]) |
           (static_cast<std::uint32_t>(src[1]) << 8) |
           (static_cast<std::uint32_t>(src[2]) << 16) |
           (static_cast<std::uint32_t>(src[3]) << 24);
}

// Read 4 bytes, most-significant first.
inline std::uint32_t readU32BE(const std::uint8_t* src) {
    return (static_cast<std::uint32_t>(src[0]) << 24) |
           (static_cast<std::uint32_t>(src[1]) << 16) |
           (static_cast<std::uint32_t>(src[2]) << 8) |
           static_cast<std::uint32_t>(src[3]);
}

// Read 8 bytes, least-significant first.
inline std::uint64_t readU64LE(const std::uint8_t* src) {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(src[i]) << (8u * i);
    }
    return result;
}

// Read 8 bytes, most-significant first.
inline std::uint64_t readU64BE(const std::uint8_t* src) {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        result |= static_cast<std::uint64_t>(src[i]) << (8u * (7 - i));
    }
    return result;
}

// --- HOST CONVERSIONS (compile-time endianness via std::endian::native) ---------
// Mixed-endian hosts are unsupported; native must be pure little or big.
static_assert(std::endian::native == std::endian::little ||
                  std::endian::native == std::endian::big,
              "maz::core::Endian requires a pure little- or big-endian host");

// True on a little-endian host, false on a big-endian host.
inline constexpr bool hostIsLittleEndian() {
    return std::endian::native == std::endian::little;
}

// Host scalar -> little-endian scalar (byte-swap on a big-endian host).
inline constexpr std::uint16_t hostToLittle16(std::uint16_t v) {
    return hostIsLittleEndian() ? v : byteSwap16(v);
}
inline constexpr std::uint32_t hostToLittle32(std::uint32_t v) {
    return hostIsLittleEndian() ? v : byteSwap32(v);
}
inline constexpr std::uint64_t hostToLittle64(std::uint64_t v) {
    return hostIsLittleEndian() ? v : byteSwap64(v);
}

// Host scalar -> big-endian scalar (byte-swap on a little-endian host).
inline constexpr std::uint16_t hostToBig16(std::uint16_t v) {
    return hostIsLittleEndian() ? byteSwap16(v) : v;
}
inline constexpr std::uint32_t hostToBig32(std::uint32_t v) {
    return hostIsLittleEndian() ? byteSwap32(v) : v;
}
inline constexpr std::uint64_t hostToBig64(std::uint64_t v) {
    return hostIsLittleEndian() ? byteSwap64(v) : v;
}

// little/big-endian scalar -> host scalar. The conversion is symmetric (byte-swap
// is an involution and host<->little is its own inverse), so these are identical
// to the matching hostToLittle/hostToBig above; both names exist for readability.
inline constexpr std::uint16_t littleToHost16(std::uint16_t v) { return hostToLittle16(v); }
inline constexpr std::uint32_t littleToHost32(std::uint32_t v) { return hostToLittle32(v); }
inline constexpr std::uint64_t littleToHost64(std::uint64_t v) { return hostToLittle64(v); }

inline constexpr std::uint16_t bigToHost16(std::uint16_t v) { return hostToBig16(v); }
inline constexpr std::uint32_t bigToHost32(std::uint32_t v) { return hostToBig32(v); }
inline constexpr std::uint64_t bigToHost64(std::uint64_t v) { return hostToBig64(v); }

} // namespace maz::core
