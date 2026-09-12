#pragma once

#include "maz/io/Varint.hpp" // appendVarint / readVarint

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::io binary diff/patch — build a compact PATCH that turns one byte buffer (the "source") into another
// (the "target"), and apply it. When two blobs are mostly the same — a save file after a few minutes of
// play, an asset re-exported with a small tweak, last tick's serialized world versus this tick's — shipping
// or storing the whole new blob is wasteful. binaryDiff finds the runs the target shares with the source and
// emits only COPY(from source) + ADD(new bytes) instructions, so a tiny change produces a tiny patch;
// binaryPatch replays those instructions to reconstruct the target EXACTLY. This is the classic
// rsync/bsdiff idea (block-hash the source, greedily match the target). It complements the engine's other
// deltas — net::writeSnapshotDelta does FIELD-level deltas of a known schema; this works on ARBITRARY bytes
// with no schema at all: patched saves, incremental asset updates, diffing opaque serialized state. The
// patch is self-contained and bounds-checked on apply (a malformed patch is rejected, never a buffer
// overrun). Header-only, std-only, deterministic. Godot has no binary diff.
namespace maz::io {

namespace detail {

inline constexpr std::size_t kDiffBlock = 16; // window size hashed for match-finding

inline std::uint64_t diffHash(const std::uint8_t* p, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull; // FNV-1a
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

} // namespace detail

// Produce a patch that transforms `src` into `dst`. Ops: 0x00 = COPY(varint srcOffset, varint len);
// 0x01 = ADD(varint len, len literal bytes).
inline std::vector<std::uint8_t> binaryDiff(const std::uint8_t* src, std::size_t srcLen,
                                            const std::uint8_t* dst, std::size_t dstLen) {
    std::vector<std::uint8_t> patch;
    const std::size_t K = detail::kDiffBlock;

    // Index the earliest source offset for each K-byte window hash.
    std::unordered_map<std::uint64_t, std::size_t> index;
    if (srcLen >= K) {
        index.reserve((srcLen - K + 1) * 2);
        for (std::size_t o = 0; o + K <= srcLen; ++o) {
            index.emplace(detail::diffHash(src + o, K), o); // keep first occurrence
        }
    }

    auto flushLiterals = [&](std::size_t from, std::size_t to) {
        if (to <= from) {
            return;
        }
        patch.push_back(0x01u); // ADD
        appendVarint(patch, to - from);
        patch.insert(patch.end(), dst + from, dst + to);
    };

    std::size_t pos = 0;
    std::size_t litStart = 0;
    while (pos < dstLen) {
        bool matched = false;
        if (pos + K <= dstLen) {
            const auto it = index.find(detail::diffHash(dst + pos, K));
            if (it != index.end()) {
                const std::size_t o = it->second;
                // Verify (guard against hash collisions).
                bool eq = true;
                for (std::size_t i = 0; i < K; ++i) {
                    if (src[o + i] != dst[pos + i]) { eq = false; break; }
                }
                if (eq) {
                    // Extend the match as far as both buffers agree.
                    std::size_t len = K;
                    while (o + len < srcLen && pos + len < dstLen && src[o + len] == dst[pos + len]) {
                        ++len;
                    }
                    flushLiterals(litStart, pos);
                    patch.push_back(0x00u); // COPY
                    appendVarint(patch, o);
                    appendVarint(patch, len);
                    pos += len;
                    litStart = pos;
                    matched = true;
                }
            }
        }
        if (!matched) {
            ++pos;
        }
    }
    flushLiterals(litStart, dstLen);
    return patch;
}

inline std::vector<std::uint8_t> binaryDiff(const std::vector<std::uint8_t>& src,
                                            const std::vector<std::uint8_t>& dst) {
    return binaryDiff(src.data(), src.size(), dst.data(), dst.size());
}

// Reconstruct the target from `src` + `patch`. Returns true and fills `out` on success; false if the patch
// is malformed or any COPY/ADD would read out of bounds (never overruns).
inline bool binaryPatch(const std::uint8_t* src, std::size_t srcLen,
                        const std::uint8_t* patch, std::size_t patchLen, std::vector<std::uint8_t>& out) {
    out.clear();
    std::size_t off = 0;
    while (off < patchLen) {
        const std::uint8_t tag = patch[off++];
        if (tag == 0x00u) { // COPY
            std::uint64_t srcOff = 0, len = 0;
            if (!readVarint(patch, patchLen, off, srcOff) || !readVarint(patch, patchLen, off, len)) {
                return false;
            }
            // Overflow-safe bounds check: srcOff and len are full 64-bit varint fields, so `srcOff + len`
            // can wrap past srcLen and slip a huge len through, after which the insert() below reads far out
            // of bounds (and OOM-grows `out`) from a malicious patch. Check srcOff first, then len against
            // the true remaining source length.
            if (srcOff > srcLen || len > srcLen - srcOff) {
                return false;
            }
            out.insert(out.end(), src + srcOff, src + srcOff + len);
        } else if (tag == 0x01u) { // ADD
            std::uint64_t len = 0;
            if (!readVarint(patch, patchLen, off, len)) {
                return false;
            }
            // Overflow-safe: `off + len` can wrap (len is a full 64-bit varint), slipping a huge len past a
            // naive check and reading out of bounds from the patch. off <= patchLen here (readVarint stops
            // within the buffer), so compare len against the true remaining patch length.
            if (len > patchLen - off) {
                return false;
            }
            out.insert(out.end(), patch + off, patch + off + len);
            off += static_cast<std::size_t>(len);
        } else {
            return false; // unknown op
        }
    }
    return true;
}

inline bool binaryPatch(const std::vector<std::uint8_t>& src, const std::vector<std::uint8_t>& patch,
                        std::vector<std::uint8_t>& out) {
    return binaryPatch(src.data(), src.size(), patch.data(), patch.size(), out);
}

} // namespace maz::io
