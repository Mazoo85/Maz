#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// maz::render GLB (binary glTF) container parser — splits a `.glb` byte buffer into its JSON and BIN chunks
// WITHOUT a file device or the cgltf dependency. A `.glb` is the single-file, self-contained form of glTF
// (the `.gltf`+`.bin`+textures packed into one blob) — the format most exporters and asset stores ship. The
// engine's `loadGltf` reads `.glb` from disk via cgltf, but pulling the two chunks out of an in-MEMORY buffer
// (say, an entry inside a `io::ResourcePack` or a byte array fetched over the network / WebSocket) needs the
// container framing itself. That framing is tiny and fixed (a 12-byte header + length-prefixed chunks), so
// this parses it directly and unit-tests headlessly against a hand-built GLB. Callers hand the extracted
// JSON to their glTF parser and index the BIN blob for buffer views.
//
// Scope note (honest): the GLB *container* (glTF 2.0, little-endian: header magic/version/length + JSON
// chunk `JSON` + optional BIN chunk `BIN\0`). Parsing the JSON scene graph and decoding the accessors is the
// glTF layer on top (`render::loadGltf`); this delivers the two raw chunks it consumes.
namespace maz::render {

struct GlbChunks {
    std::string json;               // the glTF JSON document (chunk type 0x4E4F534A "JSON")
    std::vector<std::uint8_t> bin;  // the binary buffer blob (chunk type 0x004E4942 "BIN\0"); empty if absent
    std::uint32_t version = 0;      // glTF container version (2 for glTF 2.0)
};

namespace detail {
inline std::uint32_t glbReadU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
} // namespace detail

inline constexpr std::uint32_t kGlbMagic = 0x46546C67;    // "glTF" little-endian
inline constexpr std::uint32_t kGlbChunkJson = 0x4E4F534A; // "JSON"
inline constexpr std::uint32_t kGlbChunkBin = 0x004E4942;  // "BIN\0"

// Parse a GLB byte buffer into its chunks. Returns false on a bad magic/version, a truncated header/chunk,
// or a length that overruns the buffer.
inline bool parseGlb(const std::uint8_t* d, std::size_t n, GlbChunks& out) {
    out = GlbChunks();
    if (n < 12) return false;
    if (detail::glbReadU32(d) != kGlbMagic) return false;
    const std::uint32_t version = detail::glbReadU32(d + 4);
    const std::uint32_t total = detail::glbReadU32(d + 8);
    if (version != 2) return false;
    if (total < 12 || total > n) return false; // declared length must fit the buffer

    out.version = version;
    std::size_t p = 12;
    bool haveJson = false;
    while (p + 8 <= total) {
        const std::uint32_t chunkLen = detail::glbReadU32(d + p);
        const std::uint32_t chunkType = detail::glbReadU32(d + p + 4);
        p += 8;
        if (p + chunkLen > total) return false; // chunk overruns the declared file
        if (chunkType == kGlbChunkJson) {
            out.json.assign(reinterpret_cast<const char*>(d + p), chunkLen);
            haveJson = true;
        } else if (chunkType == kGlbChunkBin) {
            out.bin.assign(d + p, d + p + chunkLen);
        }
        // Unknown chunk types are skipped per the spec (forward compatibility).
        p += chunkLen;
    }
    return haveJson; // a valid GLB must carry a JSON chunk
}

inline bool parseGlb(const std::vector<std::uint8_t>& bytes, GlbChunks& out) {
    return parseGlb(bytes.data(), bytes.size(), out);
}

// Build a GLB byte buffer from a JSON document and an optional BIN blob (the inverse — useful for packing an
// exported scene into a single self-contained file). Chunks are 4-byte aligned per the spec (JSON padded
// with spaces, BIN with zeros).
inline std::vector<std::uint8_t> buildGlb(const std::string& json, const std::vector<std::uint8_t>& bin = {}) {
    const std::size_t jsonPad = (4 - (json.size() % 4)) % 4;
    const std::size_t binPad = bin.empty() ? 0 : (4 - (bin.size() % 4)) % 4;
    const std::uint32_t jsonChunkLen = static_cast<std::uint32_t>(json.size() + jsonPad);
    const std::uint32_t binChunkLen = static_cast<std::uint32_t>(bin.size() + binPad);

    std::uint32_t total = 12 + 8 + jsonChunkLen;
    if (!bin.empty()) total += 8 + binChunkLen;

    // Sized once and filled by index, rather than reserve()-then-push_back().
    // The buffer is exactly `total` bytes and every write below is accounted
    // for in that sum, so there was never a reallocation at run time — but the
    // compiler cannot know `total` is the final size, so it keeps push_back's
    // growth path and reasons about the deallocation inside it. GCC 14 and 15
    // conclude at -O3 that `operator delete` may be called on a pointer with a
    // nonzero offset and fail the build under -Werror=free-nonheap-object.
    // Writing by index removes the growth path instead of arguing about it,
    // and leaves a single allocation. (Debug builds never saw this: the
    // diagnostic only appears once the optimiser inlines the reallocation.)
    std::vector<std::uint8_t> out(total);
    std::size_t at = 0;
    const auto putU32 = [&out, &at](std::uint32_t x) {
        out[at++] = static_cast<std::uint8_t>(x & 0xff);
        out[at++] = static_cast<std::uint8_t>((x >> 8) & 0xff);
        out[at++] = static_cast<std::uint8_t>((x >> 16) & 0xff);
        out[at++] = static_cast<std::uint8_t>((x >> 24) & 0xff);
    };

    putU32(kGlbMagic);
    putU32(2);
    putU32(total);

    putU32(jsonChunkLen);
    putU32(kGlbChunkJson);
    for (const char c : json) out[at++] = static_cast<std::uint8_t>(c);
    for (std::size_t i = 0; i < jsonPad; ++i) out[at++] = 0x20; // space pad

    if (!bin.empty()) {
        putU32(binChunkLen);
        putU32(kGlbChunkBin);
        for (const std::uint8_t b : bin) out[at++] = b;
        for (std::size_t i = 0; i < binPad; ++i) out[at++] = 0x00; // zero pad
    }
    return out;
}

} // namespace maz::render
