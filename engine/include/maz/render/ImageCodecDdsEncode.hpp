#pragma once

#include "maz/render/Image.hpp" // Image, Color

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// maz::render DDS (.dds) BC1/DXT1 encoder — the inverse of the M511 decoder, and the CPU texture-compression
// step Godot's editor runs on import (RGBA -> block-compressed GPU texture, 1/6th the memory of RGBA8). Maz
// could decode DXT but not produce it, so there was no way to author or re-pack a compressed texture on the
// CPU. `encodeBc1Block` compresses one 4x4 RGBA block to 8 bytes using range-fit endpoints (the RGB bounding
// box of the block's texels as the two 565 endpoints, then each texel snapped to the nearest of the four
// interpolated palette colours); `encodeDdsBc1` writes a complete 128-byte-header DXT1 `.dds` for an Image,
// and `saveDdsBc1` wraps it to a file. Pure CPU byte work — no GPU — so it unit-tests headlessly by
// compressing then decoding with the (independently-verified) M511 decoder and checking the result stays
// within block-compression tolerance.
//
// Scope note (honest): opaque BC1/DXT1 (4-colour mode), dimensions padded up to a multiple of 4 by clamping
// edge texels, mip-0 only. It is a fast range-fit encoder (not the optimal least-squares / cluster-fit an
// offline tool like NVTT uses), and does not emit BC2/BC3 or 1-bit-alpha punch-through. Good enough for
// authoring and round-trip; documented follow-ups for higher quality.
namespace maz::render {

namespace detail {

// Pack 8-bit RGB to 565 with rounding.
inline std::uint16_t ddsFrom565Round(int r, int g, int b) {
    const int r5 = (r * 31 + 127) / 255;
    const int g6 = (g * 63 + 127) / 255;
    const int b5 = (b * 31 + 127) / 255;
    return static_cast<std::uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

// Expand a 565 value back to 8-bit RGB (matches the decoder's dds565).
inline void ddsExpand565(std::uint16_t c, int& r, int& g, int& b) {
    const int r5 = (c >> 11) & 0x1f;
    const int g6 = (c >> 5) & 0x3f;
    const int b5 = c & 0x1f;
    r = (r5 << 3) | (r5 >> 2);
    g = (g6 << 2) | (g6 >> 4);
    b = (b5 << 3) | (b5 >> 2);
}

} // namespace detail

// Encode one 4x4 block (16 RGB texels, row-major, 3 bytes each) into 8 BC1 bytes (opaque 4-colour mode).
inline void encodeBc1Block(const std::uint8_t rgb[48], std::uint8_t out[8]) {
    // Endpoints = the two texels that are farthest apart in RGB. This "extent" range fit follows the actual
    // colour line of the block in any orientation (unlike an axis-aligned RGB bounding box, which fails when
    // channels are anticorrelated). O(16^2) — trivial per block.
    int ea = 0, eb = 0, bestPair = -1;
    for (int i = 0; i < 16; ++i) {
        for (int j = i + 1; j < 16; ++j) {
            const int dr = rgb[i * 3 + 0] - rgb[j * 3 + 0];
            const int dg = rgb[i * 3 + 1] - rgb[j * 3 + 1];
            const int db = rgb[i * 3 + 2] - rgb[j * 3 + 2];
            const int d = dr * dr + dg * dg + db * db;
            if (d > bestPair) { bestPair = d; ea = i; eb = j; }
        }
    }
    std::uint16_t c0 = detail::ddsFrom565Round(rgb[ea * 3 + 0], rgb[ea * 3 + 1], rgb[ea * 3 + 2]);
    std::uint16_t c1 = detail::ddsFrom565Round(rgb[eb * 3 + 0], rgb[eb * 3 + 1], rgb[eb * 3 + 2]);
    // Opaque DXT1 requires c0 > c1 (4-colour mode); nudge when the block is (near) constant.
    if (c0 < c1) { const std::uint16_t t = c0; c0 = c1; c1 = t; }
    if (c0 == c1) {
        if (c0 > 0) c1 = static_cast<std::uint16_t>(c0 - 1);
        else c0 = 1;
    }

    // Build the 4-colour palette exactly as the decoder does (expanded 8-bit + (2a+b)/3 interpolation).
    int pal[4][3];
    detail::ddsExpand565(c0, pal[0][0], pal[0][1], pal[0][2]);
    detail::ddsExpand565(c1, pal[1][0], pal[1][1], pal[1][2]);
    for (int k = 0; k < 3; ++k) {
        pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
        pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
    }

    std::uint32_t bits = 0;
    for (int i = 0; i < 16; ++i) {
        const int r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        int best = 0, bestD = 0x7fffffff;
        for (int p = 0; p < 4; ++p) {
            const int dr = r - pal[p][0], dg = g - pal[p][1], db = b - pal[p][2];
            const int d = dr * dr + dg * dg + db * db;
            if (d < bestD) { bestD = d; best = p; }
        }
        bits |= static_cast<std::uint32_t>(best) << (2 * i);
    }

    out[0] = static_cast<std::uint8_t>(c0 & 0xff);
    out[1] = static_cast<std::uint8_t>((c0 >> 8) & 0xff);
    out[2] = static_cast<std::uint8_t>(c1 & 0xff);
    out[3] = static_cast<std::uint8_t>((c1 >> 8) & 0xff);
    out[4] = static_cast<std::uint8_t>(bits & 0xff);
    out[5] = static_cast<std::uint8_t>((bits >> 8) & 0xff);
    out[6] = static_cast<std::uint8_t>((bits >> 16) & 0xff);
    out[7] = static_cast<std::uint8_t>((bits >> 24) & 0xff);
}

// Encode an Image to a complete DXT1 .dds byte blob (128-byte header + blocks). Returns empty on bad input.
inline std::vector<std::uint8_t> encodeDdsBc1(const Image& img) {
    const int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0) return {};
    const int bw = (w + 3) / 4, bh = (h + 3) / 4;

    std::vector<std::uint8_t> out(
        128u + static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh) * 8u, 0u);
    auto put32 = [&](std::size_t off, std::uint32_t v) {
        out[off] = static_cast<std::uint8_t>(v & 0xff);
        out[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
        out[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
        out[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
    };
    out[0] = 'D'; out[1] = 'D'; out[2] = 'S'; out[3] = ' ';
    put32(4, 124);                 // dwSize
    put32(8, 0x1007);              // flags: caps|height|width|pixelformat
    put32(12, static_cast<std::uint32_t>(h));
    put32(16, static_cast<std::uint32_t>(w));
    put32(20, static_cast<std::uint32_t>(bw * bh * 8)); // linear size
    put32(76, 32);                 // pixelformat size
    put32(80, 0x4);                // DDPF_FOURCC
    out[84] = 'D'; out[85] = 'X'; out[86] = 'T'; out[87] = '1';
    put32(108, 0x1000);            // caps: DDSCAPS_TEXTURE

    std::size_t off = 128;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            std::uint8_t block[48];
            for (int ty = 0; ty < 4; ++ty) {
                for (int tx = 0; tx < 4; ++tx) {
                    int px = bx * 4 + tx, py = by * 4 + ty;
                    if (px >= w) px = w - 1; // clamp edge texels for non-multiple-of-4 sizes
                    if (py >= h) py = h - 1;
                    const Color c = img.getPixel(px, py);
                    auto to8 = [](float f) {
                        int v = static_cast<int>(f * 255.0f + 0.5f);
                        return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
                    };
                    const int idx = (ty * 4 + tx) * 3;
                    block[idx + 0] = to8(c.r);
                    block[idx + 1] = to8(c.g);
                    block[idx + 2] = to8(c.b);
                }
            }
            encodeBc1Block(block, out.data() + off);
            off += 8;
        }
    }
    return out;
}

inline bool saveDdsBc1(const std::string& path, const Image& img) {
    const std::vector<std::uint8_t> bytes = encodeDdsBc1(img);
    if (bytes.empty()) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

} // namespace maz::render
