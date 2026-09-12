#pragma once

#include <cstdint>
#include <vector>

// maz::render KTX2 container parsing — KTX2 (Khronos Texture 2) is the standard GPU-texture file:
// it stores an ALREADY-GPU-READY image (a specific VkFormat, including GPU-compressed formats like
// BC7 / ASTC / ETC2) plus its full mip chain, so the engine uploads the bytes straight to the GPU
// with no decode. That is the whole point of compressed textures — a BC7 4K texture is ~4x smaller
// in VRAM than RGBA8 and needs no CPU unpack. Godot ships textures as .ktx2/.basis; this is the
// container reader that tells the loader what's inside and where each mip level's bytes live.
//
// This header parses the KTX2 *structure* (identifier, header, and the level index) — pure,
// std-only, bounds-checked, and unit-tested against a hand-built buffer. It does NOT transcode
// Basis-Universal supercompression or decode block formats (that needs the libktx/basisu transcoder
// and, ultimately, the GPU); it reports the format + level offsets so a GPU uploader can consume
// them. A follow-up wires this into VulkanTexture behind a real device.
namespace maz::render {

// One mip level's location within the file (and its size once any supercompression is undone).
struct Ktx2Level {
    uint64_t byteOffset = 0;
    uint64_t byteLength = 0;
    uint64_t uncompressedByteLength = 0;
};

struct Ktx2Info {
    bool valid = false;
    const char* error = "";
    uint32_t vkFormat = 0;               // the Vulkan format enum value (0 = "undefined", e.g. Basis)
    uint32_t typeSize = 0;
    uint32_t pixelWidth = 0;
    uint32_t pixelHeight = 0;
    uint32_t pixelDepth = 0;             // 0 for 2D
    uint32_t layerCount = 0;            // 0 for non-array
    uint32_t faceCount = 1;             // 6 for cubemaps
    uint32_t levelCount = 0;            // 0 means "one level" per spec
    uint32_t supercompressionScheme = 0; // 0 none, 1 BasisLZ, 2 Zstd, 3 ZLIB
    std::vector<Ktx2Level> levels;      // always >= 1 entry for a valid file

    // Effective mip count (a stored levelCount of 0 means a single, loader-generated level).
    uint32_t effectiveLevels() const { return levelCount == 0 ? 1u : levelCount; }
    bool isSupercompressed() const { return supercompressionScheme != 0; }
};

namespace detail {
inline uint32_t rdU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t rdU64(const uint8_t* p) {
    return static_cast<uint64_t>(rdU32(p)) | (static_cast<uint64_t>(rdU32(p + 4)) << 32);
}
} // namespace detail

// The 12-byte KTX2 identifier: «AB 4B 54 58 20 32 30 BB 0D 0A 1A 0A» = 0xAB "KTX 20" 0xBB \r \n \x1A \n
inline bool hasKtx2Identifier(const uint8_t* data, size_t size) {
    static const uint8_t kId[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                                    0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    if (size < 12) {
        return false;
    }
    for (int i = 0; i < 12; ++i) {
        if (data[i] != kId[i]) {
            return false;
        }
    }
    return true;
}

// Parse a KTX2 file's structure. Returns Ktx2Info{valid=false, error=...} on any malformed input
// (bad identifier, truncated header/index) rather than throwing or reading out of bounds.
inline Ktx2Info parseKtx2(const uint8_t* data, size_t size) {
    Ktx2Info info;
    if (!data) {
        info.error = "null data";
        return info;
    }
    if (!hasKtx2Identifier(data, size)) {
        info.error = "not a KTX2 file (bad identifier)";
        return info;
    }
    // Identifier (12) + header (9 x u32 = 36) + level-index header (4 x u32 + 2 x u64 = 32) = 80.
    constexpr size_t kHeaderEnd = 12 + 36 + 32;
    if (size < kHeaderEnd) {
        info.error = "truncated header";
        return info;
    }

    const uint8_t* h = data + 12;
    info.vkFormat = detail::rdU32(h + 0);
    info.typeSize = detail::rdU32(h + 4);
    info.pixelWidth = detail::rdU32(h + 8);
    info.pixelHeight = detail::rdU32(h + 12);
    info.pixelDepth = detail::rdU32(h + 16);
    info.layerCount = detail::rdU32(h + 20);
    info.faceCount = detail::rdU32(h + 24);
    info.levelCount = detail::rdU32(h + 28);
    info.supercompressionScheme = detail::rdU32(h + 32);

    if (info.pixelWidth == 0) {
        info.error = "invalid width (0)";
        return info;
    }

    const uint32_t levels = info.effectiveLevels();
    const size_t indexOffset = kHeaderEnd;                    // level index starts right after
    const size_t indexBytes = static_cast<size_t>(levels) * 24; // 3 x u64 per level
    if (size < indexOffset + indexBytes) {
        info.error = "truncated level index";
        return info;
    }

    info.levels.reserve(levels);
    for (uint32_t i = 0; i < levels; ++i) {
        const uint8_t* e = data + indexOffset + static_cast<size_t>(i) * 24;
        Ktx2Level lvl;
        lvl.byteOffset = detail::rdU64(e + 0);
        lvl.byteLength = detail::rdU64(e + 8);
        lvl.uncompressedByteLength = detail::rdU64(e + 16);
        // Sanity: a level's data must lie within the file. Written overflow-safe (a corrupt
        // byteOffset/byteLength near UINT64_MAX must not wrap past the size check).
        const uint64_t sz = static_cast<uint64_t>(size);
        if (lvl.byteLength != 0 && (lvl.byteOffset > sz || lvl.byteLength > sz - lvl.byteOffset)) {
            info.error = "level data out of bounds";
            return info;
        }
        info.levels.push_back(lvl);
    }

    info.valid = true;
    return info;
}

inline Ktx2Info parseKtx2(const std::vector<uint8_t>& bytes) {
    return parseKtx2(bytes.data(), bytes.size());
}

} // namespace maz::render
