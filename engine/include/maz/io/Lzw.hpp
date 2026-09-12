#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// maz::io — LZW (Lempel-Ziv-Welch) lossless compression. Games squeeze save files, chunked tilemaps,
// procedural data blobs, and network payloads; LZW is the classic dictionary compressor (the one behind
// GIF, TIFF, and old Unix `compress`) that finds repeated byte sequences and replaces each with a single
// code. It builds its dictionary on the fly from the data itself, so the decompressor reconstructs the
// exact same dictionary as it goes — no table needs to be stored. This implementation uses fixed 16-bit
// codes with the dictionary frozen once full (65536 entries), which keeps encode/decode trivially in
// lockstep (no variable-width code-size sync to get wrong) and is exactly invertible for ANY input. Great
// on repetitive/structured data (long runs and repeats collapse to one code); incompressible data can grow
// (each new literal costs two bytes) — that's the honest trade of this simple variant, not a match for
// DEFLATE. Godot exposes zlib/gzip; this is a dependency-free, header-only, deterministic engine-side
// compressor. Round-trip exact: decompress(compress(x)) == x.
namespace maz::io {

// Compress a byte buffer. Empty in -> empty out. Output is a little-endian stream of 16-bit codes.
inline std::vector<std::uint8_t> lzwCompress(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> out;
    if (input.empty()) {
        return out;
    }
    std::unordered_map<std::string, std::uint32_t> dict;
    dict.reserve(1u << 16);
    for (int i = 0; i < 256; ++i) {
        dict.emplace(std::string(1, static_cast<char>(i)), static_cast<std::uint32_t>(i));
    }
    std::uint32_t next = 256;
    auto emit = [&out](std::uint32_t code) {
        out.push_back(static_cast<std::uint8_t>(code & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((code >> 8) & 0xFFu));
    };
    std::string w;
    for (std::uint8_t b : input) {
        std::string wb = w;
        wb.push_back(static_cast<char>(b));
        if (dict.find(wb) != dict.end()) {
            w = std::move(wb);
        } else {
            emit(dict[w]);
            if (next < 65536u) {
                dict.emplace(std::move(wb), next++);
            }
            w = std::string(1, static_cast<char>(b));
        }
    }
    emit(dict[w]); // input is non-empty so w is a valid dictionary entry
    return out;
}

// Decompress a stream produced by lzwCompress. Empty in -> empty out. Malformed streams stop early rather
// than reading out of bounds (returns what was recovered so far).
inline std::vector<std::uint8_t> lzwDecompress(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> out;
    if (input.size() < 2) {
        return out;
    }
    std::vector<std::string> dict;
    dict.reserve(1u << 16);
    for (int i = 0; i < 256; ++i) {
        dict.emplace_back(1, static_cast<char>(i));
    }
    const std::size_t codeCount = input.size() / 2;
    auto codeAt = [&input](std::size_t k) {
        return static_cast<std::uint32_t>(input[2 * k]) |
               (static_cast<std::uint32_t>(input[2 * k + 1]) << 8);
    };
    const std::uint32_t first = codeAt(0);
    if (first >= dict.size()) {
        return out; // corrupt
    }
    std::string prev = dict[first];
    out.insert(out.end(), prev.begin(), prev.end());
    for (std::size_t k = 1; k < codeCount; ++k) {
        const std::uint32_t code = codeAt(k);
        std::string entry;
        if (code < dict.size()) {
            entry = dict[code];
        } else if (code == dict.size()) {
            entry = prev;
            entry.push_back(prev[0]); // KwKwK special case
        } else {
            break; // corrupt stream
        }
        out.insert(out.end(), entry.begin(), entry.end());
        if (dict.size() < 65536u) {
            std::string add = prev;
            add.push_back(entry[0]);
            dict.push_back(std::move(add));
        }
        prev = std::move(entry);
    }
    return out;
}

} // namespace maz::io
