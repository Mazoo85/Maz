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
    auto pushU32 = [](std::vector<std::uint8_t>& v, std::uint32_t x) {
        v.push_back(static_cast<std::uint8_t>(x & 0xff));
        v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xff));
        v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xff));
        v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xff));
    };
    const std::size_t jsonPad = (4 - (json.size() % 4)) % 4;
    const std::size_t binPad = bin.empty() ? 0 : (4 - (bin.size() % 4)) % 4;
    const std::uint32_t jsonChunkLen = static_cast<std::uint32_t>(json.size() + jsonPad);
    const std::uint32_t binChunkLen = static_cast<std::uint32_t>(bin.size() + binPad);

    std::uint32_t total = 12 + 8 + jsonChunkLen;
    if (!bin.empty()) total += 8 + binChunkLen;

    std::vector<std::uint8_t> out;
    out.reserve(total);
    pushU32(out, kGlbMagic);
    pushU32(out, 2);
    pushU32(out, total);

    pushU32(out, jsonChunkLen);
    pushU32(out, kGlbChunkJson);
    out.insert(out.end(), json.begin(), json.end());
    for (std::size_t i = 0; i < jsonPad; ++i) out.push_back(0x20); // space pad

    if (!bin.empty()) {
        pushU32(out, binChunkLen);
        pushU32(out, kGlbChunkBin);
        out.insert(out.end(), bin.begin(), bin.end());
        for (std::size_t i = 0; i < binPad; ++i) out.push_back(0x00); // zero pad
    }
    return out;
}

} // namespace maz::render
