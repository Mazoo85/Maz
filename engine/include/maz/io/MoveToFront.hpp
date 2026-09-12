#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::io Move-To-Front (MTF) coding — the stage that sits between a Burrows-Wheeler Transform and the
// entropy coder in bzip2-style compression. It keeps a list of the 256 byte values and, for each input byte,
// emits its CURRENT POSITION in that list and then moves it to the front. When the data has locally repeated
// or clustered symbols (exactly what the BWT produces), recently-seen bytes sit near the front, so their
// codes are small — long runs collapse to streams of zeros, which a following run-length + Huffman/range
// coder squeezes hard. It is perfectly reversible. Pair it with the engine's BWT and range/Huffman coders to
// complete a real compression pipeline. Header-only, std-only, deterministic.
namespace maz::io {

// Move-to-front encode: each output byte is the input byte's index in the running alphabet list.
inline std::vector<std::uint8_t> mtfEncode(const std::vector<std::uint8_t>& in) {
    std::array<std::uint8_t, 256> table;
    for (int i = 0; i < 256; ++i) {
        table[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
    }
    std::vector<std::uint8_t> out;
    out.reserve(in.size());
    for (std::uint8_t b : in) {
        // Find b's current index, emit it, then move b to the front.
        std::size_t idx = 0;
        while (table[idx] != b) {
            ++idx;
        }
        out.push_back(static_cast<std::uint8_t>(idx));
        for (std::size_t k = idx; k > 0; --k) {
            table[k] = table[k - 1];
        }
        table[0] = b;
    }
    return out;
}

// Inverse of mtfEncode.
inline std::vector<std::uint8_t> mtfDecode(const std::vector<std::uint8_t>& in) {
    std::array<std::uint8_t, 256> table;
    for (int i = 0; i < 256; ++i) {
        table[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i);
    }
    std::vector<std::uint8_t> out;
    out.reserve(in.size());
    for (std::uint8_t idx : in) {
        const std::uint8_t b = table[idx];
        out.push_back(b);
        for (std::size_t k = idx; k > 0; --k) {
            table[k] = table[k - 1];
        }
        table[0] = b;
    }
    return out;
}

} // namespace maz::io
