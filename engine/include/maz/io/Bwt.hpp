#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

// maz::io Burrows-Wheeler Transform — the reversible byte-reordering at the heart of bzip2-style compression.
// The BWT rearranges the bytes of a block so that runs of the same symbol CLUSTER together (identical
// contexts end up adjacent), which a following move-to-front + entropy coder then squeezes far better than
// the raw data — yet it loses NOTHING: the exact original is recovered from the transformed block plus one
// index. It's the standard front-end for compressing repetitive game assets (text, level data, tilemaps,
// serialized scenes) before Huffman/range coding. This is the classic rotation-sort forward transform and
// the O(n) LF-mapping inverse. The engine has Huffman, LZW and a range coder but no BWT stage; this adds it.
// Header-only, std-only, deterministic.
namespace maz::io {

struct BwtResult {
    std::vector<std::uint8_t> data; // the last column of the sorted rotation matrix
    std::size_t primaryIndex = 0;   // row of the original string among the sorted rotations
};

// Forward BWT of a byte block. Empty input yields an empty result.
inline BwtResult bwtEncode(const std::vector<std::uint8_t>& s) {
    BwtResult r;
    const std::size_t n = s.size();
    if (n == 0) {
        return r;
    }
    std::vector<std::size_t> rot(n);
    std::iota(rot.begin(), rot.end(), std::size_t{0});
    std::sort(rot.begin(), rot.end(), [&](std::size_t a, std::size_t b) {
        for (std::size_t k = 0; k < n; ++k) {
            const std::uint8_t ca = s[(a + k) % n], cb = s[(b + k) % n];
            if (ca != cb) {
                return ca < cb;
            }
        }
        return a < b; // identical rotations: stable tie-break
    });
    r.data.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        r.data[i] = s[(rot[i] + n - 1) % n]; // last column
        if (rot[i] == 0) {
            r.primaryIndex = i;
        }
    }
    return r;
}

// Inverse BWT: recover the original block from the last column + primary index.
inline std::vector<std::uint8_t> bwtDecode(const std::vector<std::uint8_t>& last, std::size_t primaryIndex) {
    const std::size_t n = last.size();
    std::vector<std::uint8_t> out;
    if (n == 0 || primaryIndex >= n) {
        return out;
    }
    // LF mapping: order the rows by their last-column symbol (stable), giving the first-column permutation.
    std::size_t count[256] = {0};
    for (std::uint8_t c : last) {
        ++count[c];
    }
    std::size_t start[256];
    std::size_t sum = 0;
    for (int b = 0; b < 256; ++b) {
        start[b] = sum;
        sum += count[b];
    }
    std::vector<std::size_t> next(n);
    std::size_t occ[256] = {0};
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t b = last[i];
        next[start[b] + occ[b]] = i;
        ++occ[b];
    }
    out.resize(n);
    std::size_t p = next[primaryIndex];
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = last[p];
        p = next[p];
    }
    return out;
}

} // namespace maz::io
