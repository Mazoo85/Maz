#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>

#include "maz/core/Assert.hpp"

// Binary-to-text codecs in maz::core — base64 (RFC 4648 standard alphabet
// `A-Za-z0-9+/` with '=' padding) and hex — for embedding binary data (e.g. an
// iter16 ByteWriter buffer) in text config/scene files. encode takes bytes and
// returns a std::string; decode REPLACES the `out` vector (it is cleared first)
// and returns false on malformed input — invalid characters, a length that is
// not a multiple of 4 (base64) / not even (hex), or misplaced '=' padding —
// WITHOUT leaving a partial result (out is cleared on failure). The Godot
// Marshalls analog. base64 here is standard, NOT URL-safe; the URL-safe alphabet
// and streaming are future refinements. Not Unicode — operates on raw bytes, and
// input chars are treated as unsigned so high-bit bytes are well-defined.
//
// Note: base64 decoding does NOT validate the unused low bits of a padded final
// symbol (RFC 4648 §3.5 makes rejecting non-canonical padding a MAY, not a MUST),
// so e.g. "Zh==" decodes to a single byte rather than being rejected. Strict
// canonical decoding is a future refinement.

namespace maz::core {

// --- BASE64 (RFC 4648 standard alphabet, '=' padding) --------------------------

// Encode bytes to standard base64. 3 input bytes -> 4 output chars; a final
// partial group of 1 byte -> 2 chars + "==", of 2 bytes -> 3 chars + "=". Empty
// input -> "".
inline std::string base64Encode(const std::vector<std::uint8_t>& data) {
    static constexpr char kB64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    const std::size_t n = data.size();
    while (i + 3 <= n) {
        const std::uint8_t b0 = data[i];
        const std::uint8_t b1 = data[i + 1];
        const std::uint8_t b2 = data[i + 2];
        out.push_back(kB64[static_cast<std::size_t>(b0 >> 2)]);
        out.push_back(kB64[static_cast<std::size_t>(((b0 & 0x03) << 4) | (b1 >> 4))]);
        out.push_back(kB64[static_cast<std::size_t>(((b1 & 0x0F) << 2) | (b2 >> 6))]);
        out.push_back(kB64[static_cast<std::size_t>(b2 & 0x3F)]);
        i += 3;
    }
    const std::size_t rem = n - i;
    if (rem == 1) {
        const std::uint8_t b0 = data[i];
        out.push_back(kB64[static_cast<std::size_t>(b0 >> 2)]);
        out.push_back(kB64[static_cast<std::size_t>((b0 & 0x03) << 4)]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const std::uint8_t b0 = data[i];
        const std::uint8_t b1 = data[i + 1];
        out.push_back(kB64[static_cast<std::size_t>(b0 >> 2)]);
        out.push_back(kB64[static_cast<std::size_t>(((b0 & 0x03) << 4) | (b1 >> 4))]);
        out.push_back(kB64[static_cast<std::size_t>((b1 & 0x0F) << 2)]);
        out.push_back('=');
    }
    return out;
}

// Decode standard base64 into `out` (REPLACED — cleared first). Length must be a
// multiple of 4 (padded base64) or the empty string. '=' padding is only valid in
// the last one or two positions of the final quartet. Any invalid character,
// wrong length, or misplaced padding -> return false with out left cleared (no
// partial result). "" -> out cleared, return true.
inline bool base64Decode(std::string_view text, std::vector<std::uint8_t>& out) {
    out.clear();
    if (text.size() % 4 != 0) { return false; }
    if (text.empty()) { return true; }

    // Reverse lookup: alphabet char -> 0..63, everything else -> sentinel 0xFF.
    constexpr std::uint8_t kInvalid = 0xFF;
    static constexpr auto kRev = [] {
        // A 256-entry table (indexed by the unsigned char value).
        struct Table { std::uint8_t v[256]; };
        Table t{};
        for (std::size_t k = 0; k < 256; ++k) { t.v[k] = kInvalid; }
        const char* alpha =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::uint8_t k = 0; k < 64; ++k) {
            t.v[static_cast<unsigned char>(alpha[k])] = k;
        }
        return t;
    }();

    out.reserve(text.size() / 4 * 3);
    const std::size_t groups = text.size() / 4;
    for (std::size_t g = 0; g < groups; ++g) {
        const bool lastGroup = (g + 1 == groups);
        const char* q = text.data() + g * 4;
        std::uint8_t sym[4];
        std::size_t pad = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const unsigned char c = static_cast<unsigned char>(q[j]);
            if (c == '=') {
                // Padding only allowed in the last two positions of the LAST group.
                if (!lastGroup || j < 2) { out.clear(); return false; }
                ++pad;
                sym[j] = 0;
            } else {
                if (pad != 0) { out.clear(); return false; }  // symbol after padding
                const std::uint8_t rv = kRev.v[c];
                if (rv == kInvalid) { out.clear(); return false; }
                sym[j] = rv;
            }
        }
        // pad can only be 0, 1, or 2 (position rules above forbid j<2).
        const std::uint32_t packed =
            (static_cast<std::uint32_t>(sym[0]) << 18) |
            (static_cast<std::uint32_t>(sym[1]) << 12) |
            (static_cast<std::uint32_t>(sym[2]) << 6) |
            static_cast<std::uint32_t>(sym[3]);
        out.push_back(static_cast<std::uint8_t>((packed >> 16) & 0xFF));
        if (pad < 2) { out.push_back(static_cast<std::uint8_t>((packed >> 8) & 0xFF)); }
        if (pad < 1) { out.push_back(static_cast<std::uint8_t>(packed & 0xFF)); }
    }
    return true;
}

// --- HEX -----------------------------------------------------------------------

// Encode each byte as two hex chars (high nibble then low). Empty -> "".
inline std::string hexEncode(const std::vector<std::uint8_t>& data, bool uppercase = false) {
    static constexpr char kLower[] = "0123456789abcdef";
    static constexpr char kUpper[] = "0123456789ABCDEF";
    const char* digits = uppercase ? kUpper : kLower;
    std::string out;
    out.reserve(data.size() * 2);
    for (const std::uint8_t b : data) {
        out.push_back(digits[static_cast<std::size_t>(b >> 4)]);
        out.push_back(digits[static_cast<std::size_t>(b & 0x0F)]);
    }
    return out;
}

// Case-insensitive nibble parse: '0'-'9'/'a'-'f'/'A'-'F' -> 0..15, else 0xFF.
inline std::uint8_t hexNibble(unsigned char c) {
    if (c >= '0' && c <= '9') { return static_cast<std::uint8_t>(c - '0'); }
    if (c >= 'a' && c <= 'f') { return static_cast<std::uint8_t>(c - 'a' + 10); }
    if (c >= 'A' && c <= 'F') { return static_cast<std::uint8_t>(c - 'A' + 10); }
    return 0xFF;
}

// Decode hex into `out` (REPLACED — cleared first). Odd length or any non-hex
// char -> return false with out left cleared. "" -> out cleared, return true.
inline bool hexDecode(std::string_view text, std::vector<std::uint8_t>& out) {
    out.clear();
    if (text.size() % 2 != 0) { return false; }
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const std::uint8_t hi = hexNibble(static_cast<unsigned char>(text[i]));
        const std::uint8_t lo = hexNibble(static_cast<unsigned char>(text[i + 1]));
        if (hi == 0xFF || lo == 0xFF) { out.clear(); return false; }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

} // namespace maz::core
