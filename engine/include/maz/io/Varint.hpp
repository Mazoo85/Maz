#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::io variable-length integer encoding (LEB128 + zigzag) — the compact way to serialize integers that
// are usually small. A fixed 4- or 8-byte field wastes space when the value is 3; a varint spends one byte
// for values under 128, two under 16384, and so on, only paying for 64 bits when the number is genuinely
// huge. This is what Protocol Buffers, WebAssembly, DWARF, and most netcode use to shrink save files, replay
// streams, delta-compressed snapshots, and network packets. Unsigned values use plain LEB128; signed values
// are first folded through zigzag so small-magnitude negatives (-1, -2, …) also encode in one byte instead
// of ten. Decoding is bounds-checked and overflow-checked: a truncated or over-long stream returns false
// rather than reading past the buffer or wrapping. Complements the engine's fixed-width StreamPeer and
// base64 (M154). Header-only, std-only, endian-independent (the byte order is defined by the format itself).
namespace maz::io {

// Number of bytes appendVarint will emit for `value` (1..10).
inline std::size_t varintSize(std::uint64_t value) {
    std::size_t n = 1;
    while (value >= 0x80u) {
        value >>= 7;
        ++n;
    }
    return n;
}

// Append `value` to `out` as unsigned LEB128: 7 payload bits per byte, high bit set on every byte but the last.
inline void appendVarint(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (;;) {
        auto byte = static_cast<std::uint8_t>(value & 0x7Fu);
        value >>= 7;
        if (value != 0u) {
            out.push_back(static_cast<std::uint8_t>(byte | 0x80u));
        } else {
            out.push_back(byte);
            return;
        }
    }
}

// Read one unsigned LEB128 value from data[offset..size), advancing `offset` past it. Returns false (leaving
// `offset` unspecified) if the stream ends mid-value or the value would exceed 64 bits.
inline bool readVarint(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint64_t& out) {
    std::uint64_t result = 0;
    int shift = 0;
    for (;;) {
        if (offset >= size) return false; // truncated
        const std::uint8_t byte = data[offset++];
        const std::uint64_t chunk = byte & 0x7Fu;
        if (shift >= 64 || (shift == 63 && chunk > 1u)) return false; // would overflow uint64
        result |= chunk << shift;
        if ((byte & 0x80u) == 0u) {
            out = result;
            return true;
        }
        shift += 7;
    }
}

// Convenience: read from a byte vector.
inline bool readVarint(const std::vector<std::uint8_t>& data, std::size_t& offset, std::uint64_t& out) {
    return readVarint(data.data(), data.size(), offset, out);
}

// ---- zigzag mapping for signed values (so small negatives stay small) ----------------------------------
// 0->0, -1->1, 1->2, -2->3, 2->4, … — folds the sign into the low bit before LEB128.
inline std::uint64_t zigzagEncode(std::int64_t v) {
    const std::uint64_t u = static_cast<std::uint64_t>(v) << 1;
    return v < 0 ? ~u : u; // XOR with an all-ones mask when negative, done branch-cheaply
}

inline std::int64_t zigzagDecode(std::uint64_t u) {
    // Written as a subtraction from zero rather than a unary minus: the value is the same,
    // but MSVC raises C4146 ("unary minus applied to unsigned type") on the latter and /WX
    // makes it fatal.
    const std::uint64_t mask = std::uint64_t{0} - (u & 1u); // 0xFFFF… if the low bit is set, else 0
    return static_cast<std::int64_t>((u >> 1) ^ mask);
}

// Append a signed value (zigzag + LEB128).
inline void appendVarintSigned(std::vector<std::uint8_t>& out, std::int64_t value) {
    appendVarint(out, zigzagEncode(value));
}

// Read a signed value (LEB128 + un-zigzag). Same failure contract as readVarint.
inline bool readVarintSigned(const std::uint8_t* data, std::size_t size, std::size_t& offset,
                             std::int64_t& out) {
    std::uint64_t u = 0;
    if (!readVarint(data, size, offset, u)) return false;
    out = zigzagDecode(u);
    return true;
}

inline bool readVarintSigned(const std::vector<std::uint8_t>& data, std::size_t& offset, std::int64_t& out) {
    return readVarintSigned(data.data(), data.size(), offset, out);
}

} // namespace maz::io
