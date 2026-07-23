#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::io — arithmetic (range) coding: entropy compression that squeezes a stream of symbols down toward its
// true information content. Where LZW replaces repeats with dictionary codes, a range coder assigns each
// symbol a slice of a numeric interval proportional to its probability, so common symbols cost a fraction of
// a bit and rare ones cost more — beating fixed-width and Huffman on skewed data (quantized audio/mesh
// residuals, save-game deltas, tile histograms). This is Dmitry Subbotin's carryless 32-bit range coder with
// a caller-supplied static frequency model (one integer weight per symbol, total <= 65536). Godot exposes
// zlib/gzip but no arithmetic coder. Header-only, std-only, deterministic; encode/decode round-trip exactly.
namespace maz::io {

namespace detail {
constexpr std::uint32_t kRangeTop = 1u << 24;
constexpr std::uint32_t kRangeBot = 1u << 16;

// Cumulative table (size n+1) from per-symbol frequencies (each >= 1).
inline std::vector<std::uint32_t> cumulate(const std::vector<std::uint32_t>& freq) {
    std::vector<std::uint32_t> cum(freq.size() + 1, 0);
    for (std::size_t i = 0; i < freq.size(); ++i) {
        cum[i + 1] = cum[i] + (freq[i] == 0 ? 1u : freq[i]);
    }
    return cum;
}
} // namespace detail

// Encode `symbols` (indices into the `freq` table) into a byte stream. Prepends the symbol count so the
// decoder knows when to stop. Returns empty for an empty/invalid model.
inline std::vector<std::uint8_t> rangeEncode(const std::vector<std::uint32_t>& symbols,
                                             const std::vector<std::uint32_t>& freq) {
    std::vector<std::uint8_t> out;
    if (freq.empty()) {
        return out;
    }
    const std::vector<std::uint32_t> cum = detail::cumulate(freq);
    const std::uint32_t total = cum.back();
    if (total == 0 || total > detail::kRangeBot) {
        return out;
    }
    // 4-byte little-endian symbol count header.
    const std::uint32_t n = static_cast<std::uint32_t>(symbols.size());
    out.push_back(static_cast<std::uint8_t>(n & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((n >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((n >> 24) & 0xFFu));

    std::uint32_t low = 0, range = 0xFFFFFFFFu;
    for (std::uint32_t s : symbols) {
        if (s >= freq.size()) {
            return std::vector<std::uint8_t>{};
        }
        range /= total;
        low += cum[s] * range;
        range *= (cum[s + 1] - cum[s]);
        for (;;) {
            if ((low ^ (low + range)) < detail::kRangeTop) {
                // top byte settled
            } else if (range < detail::kRangeBot) {
                range = (0u - low) & (detail::kRangeBot - 1u); // underflow: force a renorm
            } else {
                break;
            }
            out.push_back(static_cast<std::uint8_t>(low >> 24));
            low <<= 8;
            range <<= 8;
        }
    }
    for (int i = 0; i < 4; ++i) { // flush
        out.push_back(static_cast<std::uint8_t>(low >> 24));
        low <<= 8;
    }
    return out;
}

// Decode a stream produced by rangeEncode with the SAME `freq` model. Returns the recovered symbols.
inline std::vector<std::uint32_t> rangeDecode(const std::vector<std::uint8_t>& data,
                                              const std::vector<std::uint32_t>& freq) {
    std::vector<std::uint32_t> out;
    if (freq.empty() || data.size() < 4) {
        return out;
    }
    const std::vector<std::uint32_t> cum = detail::cumulate(freq);
    const std::uint32_t total = cum.back();
    if (total == 0 || total > detail::kRangeBot) {
        return out;
    }
    const std::uint32_t n = static_cast<std::uint32_t>(data[0]) |
                            (static_cast<std::uint32_t>(data[1]) << 8) |
                            (static_cast<std::uint32_t>(data[2]) << 16) |
                            (static_cast<std::uint32_t>(data[3]) << 24);
    std::size_t pos = 4;
    auto nextByte = [&]() -> std::uint32_t { return pos < data.size() ? data[pos++] : 0u; };

    std::uint32_t low = 0, range = 0xFFFFFFFFu, code = 0;
    for (int i = 0; i < 4; ++i) {
        code = (code << 8) | nextByte();
    }
    out.reserve(n);
    for (std::uint32_t k = 0; k < n; ++k) {
        range /= total;
        const std::uint32_t value = (code - low) / range;
        // Find symbol s with cum[s] <= value < cum[s+1] (linear over a small alphabet).
        std::uint32_t s = 0;
        while (s + 1 < freq.size() && cum[s + 1] <= value) {
            ++s;
        }
        out.push_back(s);
        low += cum[s] * range;
        range *= (cum[s + 1] - cum[s]);
        for (;;) {
            if ((low ^ (low + range)) < detail::kRangeTop) {
                // settled
            } else if (range < detail::kRangeBot) {
                range = (0u - low) & (detail::kRangeBot - 1u);
            } else {
                break;
            }
            code = (code << 8) | nextByte();
            low <<= 8;
            range <<= 8;
        }
    }
    return out;
}

} // namespace maz::io
