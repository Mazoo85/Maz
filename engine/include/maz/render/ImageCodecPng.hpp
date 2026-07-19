#pragma once

#include "maz/io/Inflate.hpp"
#include "maz/render/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// maz::render PNG (.png) decoder — closes the single most important image-import gap versus Godot, which
// imports PNG everywhere. Built on the M499 `io::zlibInflate` decompressor: it walks the PNG chunk stream
// (IHDR / PLTE / tRNS / IDAT / IEND), inflates the concatenated IDAT data, reverses the five per-scanline
// filters (None / Sub / Up / Average / Paeth), and expands the samples into an RGBA8 `Image` ready for
// `Renderer::createTexture`. Grayscale, RGB, RGBA, grayscale+alpha, and 8-bit palette (with optional tRNS
// alpha) color types are supported. Pure CPU byte work — unit-tested headlessly against PNGs produced by a
// reference encoder.
//
// Scope note (honest): 8-bits-per-channel, both progressive and Adam7-interlaced. It does not handle
// 1/2/4/16-bit depths or ancillary color-management chunks; those are documented follow-ups. A malformed or
// unsupported file returns an empty Image.
namespace maz::render {

namespace detail {

inline std::uint32_t pngU32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

inline int pngPaeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

} // namespace detail

inline Image decodePng(const std::uint8_t* data, std::size_t size) {
    static const std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8) return Image{};
    for (int i = 0; i < 8; ++i)
        if (data[i] != kSig[i]) return Image{};

    std::size_t pos = 8;
    int width = 0, height = 0, bitDepth = 0, colorType = -1, interlace = 0;
    std::vector<std::uint8_t> palette; // RGB triples
    std::vector<std::uint8_t> transAlpha; // per-index alpha (tRNS for palette images)
    std::vector<std::uint8_t> idat;

    while (pos + 8 <= size) {
        const std::uint32_t len = detail::pngU32(data + pos);
        const std::uint8_t* type = data + pos + 4;
        const std::size_t body = pos + 8;
        if (body + len + 4 > size) return Image{}; // truncated chunk (incl. CRC)
        auto is = [&](const char* s) {
            return type[0] == static_cast<std::uint8_t>(s[0]) && type[1] == static_cast<std::uint8_t>(s[1]) &&
                   type[2] == static_cast<std::uint8_t>(s[2]) && type[3] == static_cast<std::uint8_t>(s[3]);
        };
        if (is("IHDR")) {
            if (len < 13) return Image{};
            width = static_cast<int>(detail::pngU32(data + body));
            height = static_cast<int>(detail::pngU32(data + body + 4));
            bitDepth = data[body + 8];
            colorType = data[body + 9];
            interlace = data[body + 12];
        } else if (is("PLTE")) {
            palette.assign(data + body, data + body + len);
        } else if (is("tRNS")) {
            transAlpha.assign(data + body, data + body + len);
        } else if (is("IDAT")) {
            idat.insert(idat.end(), data + body, data + body + len);
        } else if (is("IEND")) {
            break;
        }
        pos = body + len + 4; // advance past data + CRC
    }

    if (width <= 0 || height <= 0 || bitDepth != 8 || interlace > 1) return Image{};

    int channels = 0;
    switch (colorType) {
        case 0: channels = 1; break; // grayscale
        case 2: channels = 3; break; // RGB
        case 3: channels = 1; break; // palette index
        case 4: channels = 2; break; // grayscale + alpha
        case 6: channels = 4; break; // RGBA
        default: return Image{};
    }
    if (colorType == 3 && palette.empty()) return Image{};

    std::vector<std::uint8_t> raw;
    if (!io::zlibInflate(idat, raw)) return Image{};

    const std::size_t bpp = static_cast<std::size_t>(channels);
    std::size_t src = 0;
    bool failed = false;

    // Unfilter a `sw x sh` sub-image starting at the current stream cursor into a fresh buffer, advancing
    // the cursor. Returns an empty buffer (and sets `failed`) on a bad filter byte or a short stream.
    auto unfilterSub = [&](int sw, int sh) -> std::vector<std::uint8_t> {
        const std::size_t sstride = static_cast<std::size_t>(sw) * bpp;
        std::vector<std::uint8_t> rec(static_cast<std::size_t>(sh) * sstride, 0u);
        for (int y = 0; y < sh; ++y) {
            if (src + 1 + sstride > raw.size()) {
                failed = true;
                return {};
            }
            const std::uint8_t filter = raw[src++];
            const std::size_t rowOff = static_cast<std::size_t>(y) * sstride;
            for (std::size_t i = 0; i < sstride; ++i) {
                const int x = raw[src++];
                const int a = i >= bpp ? rec[rowOff + i - bpp] : 0;
                const int b = y > 0 ? rec[rowOff - sstride + i] : 0;
                const int c = (i >= bpp && y > 0) ? rec[rowOff - sstride + i - bpp] : 0;
                int v = x;
                switch (filter) {
                    case 0: v = x; break;
                    case 1: v = x + a; break;
                    case 2: v = x + b; break;
                    case 3: v = x + ((a + b) >> 1); break;
                    case 4: v = x + detail::pngPaeth(a, b, c); break;
                    default: failed = true; return {};
                }
                rec[rowOff + i] = static_cast<std::uint8_t>(v & 0xff);
            }
        }
        return rec;
    };

    Image img(width, height);

    // Expand one sub-image sample (at rec[s]) to RGBA and write it to (dx, dy).
    auto writeSample = [&](const std::vector<std::uint8_t>& rec, std::size_t s, int dx, int dy) {
        int r = 0, g = 0, b = 0, al = 255;
        switch (colorType) {
            case 0: r = g = b = rec[s]; break;
            case 2: r = rec[s]; g = rec[s + 1]; b = rec[s + 2]; break;
            case 4: r = g = b = rec[s]; al = rec[s + 1]; break;
            case 6: r = rec[s]; g = rec[s + 1]; b = rec[s + 2]; al = rec[s + 3]; break;
            case 3: {
                const std::size_t idxp = static_cast<std::size_t>(rec[s]) * 3u;
                if (idxp + 2 < palette.size()) {
                    r = palette[idxp];
                    g = palette[idxp + 1];
                    b = palette[idxp + 2];
                }
                if (rec[s] < transAlpha.size()) al = transAlpha[rec[s]];
                break;
            }
            default: break;
        }
        img.setPixel(dx, dy, color8(r, g, b, al));
    };

    if (interlace == 0) {
        const std::vector<std::uint8_t> rec = unfilterSub(width, height);
        if (failed) return Image{};
        const std::size_t stride = static_cast<std::size_t>(width) * bpp;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                writeSample(rec, static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * bpp,
                            x, y);
    } else {
        // Adam7: seven passes, each {startX, startY, stepX, stepY}, scattered into the full image.
        static const int adam[7][4] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4},
                                       {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
        for (const auto& p : adam) {
            const int sx = p[0], sy = p[1], stx = p[2], sty = p[3];
            const int sw = (width - sx + stx - 1) / stx;
            const int sh = (height - sy + sty - 1) / sty;
            if (sw <= 0 || sh <= 0) continue;
            const std::vector<std::uint8_t> rec = unfilterSub(sw, sh);
            if (failed) return Image{};
            const std::size_t sstride = static_cast<std::size_t>(sw) * bpp;
            for (int r = 0; r < sh; ++r)
                for (int c = 0; c < sw; ++c)
                    writeSample(rec, static_cast<std::size_t>(r) * sstride + static_cast<std::size_t>(c) * bpp,
                                sx + c * stx, sy + r * sty);
        }
    }
    return img;
}

inline Image decodePng(const std::vector<std::uint8_t>& bytes) {
    return decodePng(bytes.data(), bytes.size());
}

inline Image loadPng(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return Image{};
    const std::streamsize size = f.tellg();
    if (size < 0) return Image{};
    f.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);
    return decodePng(bytes);
}

} // namespace maz::render
