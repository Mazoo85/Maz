#pragma once

#include <cstdint>
#include <string>
#include <vector>

// maz::io byte compression — a small, self-contained LZSS (LZ77 + sliding window) codec for shrinking
// save files, level data, and network payloads. It scans for the longest run of bytes already seen in
// a 4 KB back-window and replaces it with a compact (offset, length) reference; runs too short to pay
// for a reference are emitted as literals. Output is a stream of groups, each a control byte of eight
// flag bits (LSB first: 1 = the next literal byte, 0 = a 2-byte match token of a 12-bit offset and a
// 4-bit length). This is Godot's PackedByteArray.compress/decompress territory (a lossless general
// codec) — round-trip-exact for any input, not a specific on-disk format. Header-only, dependency-free.
namespace maz::io {

namespace detail {
inline constexpr std::size_t kLzWindow = 4096;   // 12-bit offsets (1..4096)
inline constexpr std::size_t kLzMinMatch = 3;    // shorter runs are cheaper as literals
inline constexpr std::size_t kLzMaxMatch = 18;   // minMatch + 15 (4-bit length field)
} // namespace detail

// Compress `data` into an LZSS byte stream. Empty input yields empty output.
inline std::vector<std::uint8_t> lzCompress(const std::uint8_t* data, std::size_t n) {
    std::vector<std::uint8_t> out;
    std::size_t pos = 0;
    while (pos < n) {
        const std::size_t controlIndex = out.size();
        out.push_back(0); // placeholder control byte
        std::uint8_t control = 0;
        for (int bit = 0; bit < 8 && pos < n; ++bit) {
            const std::size_t windowStart = (pos > detail::kLzWindow) ? pos - detail::kLzWindow : 0;
            std::size_t bestLen = 0;
            std::size_t bestOff = 0;
            for (std::size_t j = windowStart; j < pos; ++j) {
                std::size_t len = 0;
                while (len < detail::kLzMaxMatch && pos + len < n && data[j + len] == data[pos + len]) {
                    ++len;
                }
                if (len > bestLen) {
                    bestLen = len;
                    bestOff = pos - j;
                }
            }
            if (bestLen >= detail::kLzMinMatch) {
                const std::uint16_t token = static_cast<std::uint16_t>(
                    ((static_cast<std::uint16_t>(bestOff - 1) & 0x0FFF) << 4) |
                    (static_cast<std::uint16_t>(bestLen - detail::kLzMinMatch) & 0x0F));
                out.push_back(static_cast<std::uint8_t>(token >> 8));
                out.push_back(static_cast<std::uint8_t>(token & 0xFF));
                pos += bestLen;
                // control bit stays 0 (a match)
            } else {
                control = static_cast<std::uint8_t>(control | (1u << bit)); // literal
                out.push_back(data[pos]);
                ++pos;
            }
        }
        out[controlIndex] = control;
    }
    return out;
}

inline std::vector<std::uint8_t> lzCompress(const std::vector<std::uint8_t>& data) {
    return lzCompress(data.data(), data.size());
}

// Decompress an LZSS stream produced by lzCompress. Truncated/garbage input decodes what it safely can
// and stops (it never reads out of bounds).
inline std::vector<std::uint8_t> lzDecompress(const std::uint8_t* data, std::size_t n) {
    std::vector<std::uint8_t> out;
    std::size_t pos = 0;
    while (pos < n) {
        const std::uint8_t control = data[pos++];
        for (int bit = 0; bit < 8 && pos < n; ++bit) {
            if (control & (1u << bit)) {
                out.push_back(data[pos++]); // literal
            } else {
                if (pos + 1 >= n) {
                    return out; // truncated match token
                }
                const std::uint16_t token =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[pos]) << 8) |
                                               static_cast<std::uint16_t>(data[pos + 1]));
                pos += 2;
                const std::size_t off = static_cast<std::size_t>(token >> 4) + 1;
                const std::size_t len = static_cast<std::size_t>(token & 0x0F) + detail::kLzMinMatch;
                if (off > out.size()) {
                    return out; // corrupt back-reference
                }
                const std::size_t start = out.size() - off;
                for (std::size_t k = 0; k < len; ++k) {
                    out.push_back(out[start + k]); // byte-by-byte so overlapping runs work
                }
            }
        }
    }
    return out;
}

inline std::vector<std::uint8_t> lzDecompress(const std::vector<std::uint8_t>& data) {
    return lzDecompress(data.data(), data.size());
}

// String convenience wrappers.
inline std::vector<std::uint8_t> lzCompress(const std::string& s) {
    return lzCompress(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}
inline std::string lzDecompressToString(const std::vector<std::uint8_t>& data) {
    const std::vector<std::uint8_t> bytes = lzDecompress(data);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace maz::io
