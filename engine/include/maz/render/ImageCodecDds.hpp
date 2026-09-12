#pragma once

#include "maz/render/Image.hpp" // Image, color8

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// maz::render DDS (.dds) decoder — unpacks the block-compressed DirectDraw Surface textures that games
// ship by the thousand (DXT1/DXT3/DXT5, a.k.a. BC1/BC2/BC3) into an editable RGBA8 `Image`. Godot's Image
// importer reads DDS; Maz's Ktx2 path keeps compressed blocks for direct GPU upload and never unpacks them
// on the CPU, so there was no way to get DDS pixels into an `Image` for procedural editing, thumbnails, or
// software sampling. This decoder does exactly that: it reads the 128-byte DDS header, walks the 4x4 block
// grid, and reverses the S3TC/BC block math (565 color endpoints + 2-bit selectors, plus BC2's explicit
// 4-bit alpha or BC3's interpolated 3-bit alpha). Pure CPU byte work — no GPU — so it unit-tests headlessly
// against blocks produced by a reference decoder; `loadDds` wraps it for files.
//
// Scope note (honest): the three S3TC block formats (DXT1/DXT3/DXT5) with the classic 128-byte header, the
// mip-0 top surface only. Uncompressed-RGB DDS, DX10-extended-header formats (BC4-7, ASTC), cubemaps, and
// mip chains are documented follow-ups. Colour interpolation uses the standard (2a+b)/3 rule on the expanded
// 8-bit endpoints; exact rounding of interpolated texels is hardware-defined and may differ by ±1. A
// malformed or unsupported file returns an empty Image.
namespace maz::render {

namespace detail {

inline std::uint32_t ddsU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Expand a 16-bit 565 color to 8-bit RGB.
inline void dds565(std::uint16_t c, int& r, int& g, int& b) {
    const int r5 = (c >> 11) & 0x1f;
    const int g6 = (c >> 5) & 0x3f;
    const int b5 = c & 0x1f;
    r = (r5 << 3) | (r5 >> 2);
    g = (g6 << 2) | (g6 >> 4);
    b = (b5 << 3) | (b5 >> 2);
}

// Decode a BC1/S3TC color block (8 bytes) into 16 RGB triples + 16 alpha flags. When `dxt1` and the first
// endpoint is not greater than the second, index 3 is the transparent-black "punch-through" texel.
inline void ddsColorBlock(const std::uint8_t* blk, int rgb[16][3], int alpha[16], bool dxt1) {
    const std::uint16_t c0 = static_cast<std::uint16_t>(blk[0] | (blk[1] << 8));
    const std::uint16_t c1 = static_cast<std::uint16_t>(blk[2] | (blk[3] << 8));
    int pal[4][3];
    int palA[4] = {255, 255, 255, 255};
    dds565(c0, pal[0][0], pal[0][1], pal[0][2]);
    dds565(c1, pal[1][0], pal[1][1], pal[1][2]);
    if (!dxt1 || c0 > c1) {
        for (int k = 0; k < 3; ++k) {
            pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
            pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
        }
    } else {
        for (int k = 0; k < 3; ++k) {
            pal[2][k] = (pal[0][k] + pal[1][k]) / 2;
            pal[3][k] = 0;
        }
        palA[3] = 0; // 1-bit alpha: index 3 is transparent
    }
    const std::uint32_t bits = ddsU32(blk + 4);
    for (int i = 0; i < 16; ++i) {
        const int idx = static_cast<int>((bits >> (2 * i)) & 0x3);
        rgb[i][0] = pal[idx][0];
        rgb[i][1] = pal[idx][1];
        rgb[i][2] = pal[idx][2];
        alpha[i] = palA[idx];
    }
}

// Decode a BC3/DXT5 alpha block (8 bytes) into 16 alpha values.
inline void ddsAlphaBlockBc3(const std::uint8_t* blk, int alpha[16]) {
    const int a0 = blk[0], a1 = blk[1];
    int pal[8];
    pal[0] = a0;
    pal[1] = a1;
    if (a0 > a1) {
        for (int k = 1; k <= 6; ++k) pal[k + 1] = ((7 - k) * a0 + k * a1) / 7;
    } else {
        for (int k = 1; k <= 4; ++k) pal[k + 1] = ((5 - k) * a0 + k * a1) / 5;
        pal[6] = 0;
        pal[7] = 255;
    }
    // 48 bits of 3-bit indices, little-endian across the six bytes blk[2..7].
    std::uint64_t bits = 0;
    for (int k = 0; k < 6; ++k) bits |= static_cast<std::uint64_t>(blk[2 + k]) << (8 * k);
    for (int i = 0; i < 16; ++i) {
        const int idx = static_cast<int>((bits >> (3 * i)) & 0x7);
        alpha[i] = pal[idx];
    }
}

// Decode a BC2/DXT3 alpha block (8 bytes) into 16 alpha values (explicit 4-bit alpha, scaled to 8-bit).
inline void ddsAlphaBlockBc2(const std::uint8_t* blk, int alpha[16]) {
    for (int i = 0; i < 16; ++i) {
        const int nib = (blk[i / 2] >> (4 * (i & 1))) & 0xf;
        alpha[i] = nib * 17; // 0..15 -> 0..255
    }
}

} // namespace detail

inline Image decodeDds(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 128) return Image{};
    if (!(data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')) return Image{};
    if (detail::ddsU32(data + 4) != 124) return Image{}; // dwSize

    const int height = static_cast<int>(detail::ddsU32(data + 12));
    const int width = static_cast<int>(detail::ddsU32(data + 16));
    const std::uint8_t* cc = data + 84; // fourCC in DDS_PIXELFORMAT
    if (width <= 0 || height <= 0) return Image{};

    int mode = 0; // 1 = DXT1, 2 = DXT3, 3 = DXT5
    int blockBytes = 0;
    if (cc[0] == 'D' && cc[1] == 'X' && cc[2] == 'T') {
        if (cc[3] == '1') { mode = 1; blockBytes = 8; }
        else if (cc[3] == '3') { mode = 2; blockBytes = 16; }
        else if (cc[3] == '5') { mode = 3; blockBytes = 16; }
    }
    if (mode == 0) return Image{}; // only the S3TC block formats are supported

    const int blocksWide = (width + 3) / 4;
    const int blocksHigh = (height + 3) / 4;
    const std::size_t need = static_cast<std::size_t>(blocksWide) * static_cast<std::size_t>(blocksHigh) *
                             static_cast<std::size_t>(blockBytes);
    if (128u + need > size) return Image{};

    Image img(width, height);
    std::size_t off = 128;
    for (int by = 0; by < blocksHigh; ++by) {
        for (int bx = 0; bx < blocksWide; ++bx) {
            const std::uint8_t* blk = data + off;
            int alpha[16];
            for (int i = 0; i < 16; ++i) alpha[i] = 255;
            const std::uint8_t* colorBlk = blk;
            if (mode == 2) {
                detail::ddsAlphaBlockBc2(blk, alpha);
                colorBlk = blk + 8;
            } else if (mode == 3) {
                detail::ddsAlphaBlockBc3(blk, alpha);
                colorBlk = blk + 8;
            }
            int rgb[16][3];
            int colA[16];
            detail::ddsColorBlock(colorBlk, rgb, colA, mode == 1);
            for (int ty = 0; ty < 4; ++ty) {
                for (int tx = 0; tx < 4; ++tx) {
                    const int px = bx * 4 + tx;
                    const int py = by * 4 + ty;
                    if (px >= width || py >= height) continue;
                    const int t = ty * 4 + tx;
                    const int a = (mode == 1) ? colA[t] : alpha[t];
                    img.setPixel(px, py, color8(rgb[t][0], rgb[t][1], rgb[t][2], a));
                }
            }
            off += static_cast<std::size_t>(blockBytes);
        }
    }
    return img;
}

inline Image decodeDds(const std::vector<std::uint8_t>& bytes) {
    return decodeDds(bytes.data(), bytes.size());
}

inline Image loadDds(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return Image{};
    const std::streamsize size = f.tellg();
    if (size < 0) return Image{};
    f.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (size > 0) f.read(reinterpret_cast<char*>(bytes.data()), size);
    return decodeDds(bytes);
}

} // namespace maz::render
