#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace maz::io {

// Base64 — Godot's Marshalls raw_to_base64 / base64_to_raw. The standard way to carry BINARY data through
// TEXT channels: embed a texture, a save blob, or any byte buffer inside a JSON string, a .tres/.tscn
// resource, a URL, or a config value. Encoding maps every 3 bytes to 4 ASCII characters (A–Z a–z 0–9 + /)
// with '=' padding; decoding reverses it, tolerating embedded whitespace/newlines (so wrapped blobs decode)
// and rejecting stray non-alphabet characters. Standard RFC 4648 alphabet. Header-only, deterministic — it
// unit-tests exactly against the canonical vectors and drives a golden text readout.

inline std::string base64Encode(const std::uint8_t* data, std::size_t n) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((n + 2) / 3) * 4);
    std::size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kTable[(v >> 18) & 63]);
        out.push_back(kTable[(v >> 12) & 63]);
        out.push_back(kTable[(v >> 6) & 63]);
        out.push_back(kTable[v & 63]);
    }
    const std::size_t rem = n - i;
    if (rem == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kTable[(v >> 18) & 63]);
        out.push_back(kTable[(v >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out.push_back(kTable[(v >> 18) & 63]);
        out.push_back(kTable[(v >> 12) & 63]);
        out.push_back(kTable[(v >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

inline std::string base64Encode(const std::vector<std::uint8_t>& data) {
    return base64Encode(data.data(), data.size());
}

inline std::string base64Encode(const std::string& text) {
    return base64Encode(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

// Map a base64 character to its 6-bit value, or -1 if it is not an alphabet character.
inline int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

// Decode base64 text into `out`. Whitespace (space/tab/CR/LF) is skipped so line-wrapped blobs decode; '='
// padding ends the data. Returns false (and leaves `out` with whatever decoded so far) on a stray
// non-alphabet, non-whitespace character.
inline bool base64Decode(const std::string& text, std::vector<std::uint8_t>& out) {
    out.clear();
    out.reserve(text.size() / 4 * 3 + 3);
    std::uint32_t acc = 0;
    int bits = 0;
    for (const char c : text) {
        if (c == '=') {
            break; // padding — the remaining bits are zero-fill, discard them
        }
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            continue;
        }
        const int v = base64Value(c);
        if (v < 0) {
            return false;
        }
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
        }
    }
    return true;
}

// Convenience: decode to a fresh vector (empty on error).
inline std::vector<std::uint8_t> base64Decode(const std::string& text) {
    std::vector<std::uint8_t> out;
    if (!base64Decode(text, out)) {
        out.clear();
    }
    return out;
}

} // namespace maz::io
