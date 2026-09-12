#pragma once

#include "maz/core/Hash.hpp"   // crc32
#include "maz/io/Inflate.hpp"  // inflateRaw

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::io gzip (.gz, RFC 1952) decompressor — the container Godot reads through FileAccess's gzip compression
// mode and that wraps countless downloaded/shipped assets (`.gz`). Maz already had raw-DEFLATE and zlib
// inflate (M499) but not the gzip framing on top, so a `.gz` blob could not be opened. `gunzip` parses the
// 10-byte gzip header (magic 1f 8b, method 08) plus the optional FEXTRA / FNAME / FCOMMENT / FHCRC fields,
// runs the embedded DEFLATE stream through the existing `inflateRaw`, and then VERIFIES the result against
// the trailing CRC-32 (via the existing `core::crc32`) and ISIZE footer — so a corrupted or truncated stream
// is reported rather than silently returning garbage. Pure CPU, header-only, unit-tested against blobs
// produced by Python's reference `gzip` module.
//
// Scope note (honest): single-member gzip streams (the overwhelmingly common case); concatenated multi-member
// streams decode only their first member. Decompress only — no gzip *compression* (Maz's inflate side has no
// deflate encoder yet). Optional header fields are skipped correctly but not returned to the caller.
namespace maz::io {

// Decompress a single-member gzip stream into `out`. Returns false on a bad header, a short/truncated stream,
// a DEFLATE error, or a CRC-32 / ISIZE mismatch.
inline bool gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out) {
    out.clear();
    if (!data || size < 18) return false;                 // 10 header + 8 footer minimum
    if (data[0] != 0x1f || data[1] != 0x8b || data[2] != 0x08) return false; // magic + deflate method
    const std::uint8_t flg = data[3];

    std::size_t pos = 10; // past the fixed 10-byte header
    const std::size_t footer = size - 8; // CRC32 (4) + ISIZE (4)

    if (flg & 0x04) { // FEXTRA: 2-byte length + payload
        if (pos + 2 > footer) return false;
        const std::size_t xlen = static_cast<std::size_t>(data[pos]) | (static_cast<std::size_t>(data[pos + 1]) << 8);
        pos += 2 + xlen;
    }
    if (flg & 0x08) { // FNAME: zero-terminated
        while (pos < footer && data[pos] != 0) ++pos;
        ++pos; // skip the NUL
    }
    if (flg & 0x10) { // FCOMMENT: zero-terminated
        while (pos < footer && data[pos] != 0) ++pos;
        ++pos;
    }
    if (flg & 0x02) pos += 2; // FHCRC: 2-byte header CRC16
    if (pos > footer) return false;

    if (!inflateRaw(data + pos, footer - pos, out)) return false;

    // Verify CRC-32 of the uncompressed data and the ISIZE (length mod 2^32) footer.
    const std::uint32_t wantCrc = static_cast<std::uint32_t>(data[footer]) |
                                  (static_cast<std::uint32_t>(data[footer + 1]) << 8) |
                                  (static_cast<std::uint32_t>(data[footer + 2]) << 16) |
                                  (static_cast<std::uint32_t>(data[footer + 3]) << 24);
    const std::uint32_t wantSize = static_cast<std::uint32_t>(data[footer + 4]) |
                                   (static_cast<std::uint32_t>(data[footer + 5]) << 8) |
                                   (static_cast<std::uint32_t>(data[footer + 6]) << 16) |
                                   (static_cast<std::uint32_t>(data[footer + 7]) << 24);
    if (static_cast<std::uint32_t>(out.size() & 0xffffffffu) != wantSize) { out.clear(); return false; }
    if (core::crc32(out.data(), out.size()) != wantCrc) { out.clear(); return false; }
    return true;
}

inline bool gunzip(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out) {
    return gunzip(bytes.data(), bytes.size(), out);
}

} // namespace maz::io
