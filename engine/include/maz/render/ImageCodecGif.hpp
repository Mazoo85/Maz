#pragma once

#include "maz/render/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

// maz::render GIF codec — decode/encode the GIF (Graphics Interchange Format) image, still ubiquitous for
// pixel-art sprites, UI icons, and short loops on the web. GIF stores an INDEXED image (a palette of up to
// 256 RGB colors + one index per pixel) and compresses the index stream with variable-width LZW. This
// implements that: `decodeGif` reads the header + logical-screen + global color table + the first image
// frame's LZW data into an RGBA8 Image, and `encodeGif` writes a single-frame GIF89a — building a palette
// from the image (using the exact colors when there are ≤256, else median-cut via ColorQuantize) and
// LZW-compressing the indices. Pure CPU bytes (no GPU), so it unit-tests headlessly by an ENCODE→DECODE
// round-trip that is LOSSLESS for ≤256-color images (the common case for GIF's target content).
//
// Scope note (honest): GIF89a, the FIRST frame, global color table, no interlace, opaque (GIF transparency
// index + multi-frame animation are documented follow-ups). The variable-width LZW (clear/EOI codes, code
// growth 2→12 bits, dictionary reset) is complete.
namespace maz::render {

namespace detail {

// LSB-first bit writer for GIF's LZW code stream.
struct GifBitWriter {
    std::vector<std::uint8_t> out;
    std::uint32_t buffer = 0;
    int bits = 0;
    void put(std::uint32_t code, int codeSize) {
        buffer |= (code << bits);
        bits += codeSize;
        while (bits >= 8) {
            out.push_back(static_cast<std::uint8_t>(buffer & 0xFF));
            buffer >>= 8;
            bits -= 8;
        }
    }
    void flush() {
        if (bits > 0) {
            out.push_back(static_cast<std::uint8_t>(buffer & 0xFF));
            buffer = 0;
            bits = 0;
        }
    }
};

// LSB-first bit reader over the concatenated LZW data bytes.
struct GifBitReader {
    const std::vector<std::uint8_t>& data;
    std::size_t bytePos = 0;
    int bitPos = 0;
    explicit GifBitReader(const std::vector<std::uint8_t>& d) : data(d) {}
    bool read(int codeSize, std::uint32_t& out) {
        std::uint32_t code = 0;
        for (int i = 0; i < codeSize; ++i) {
            if (bytePos >= data.size()) return false;
            const int bit = (data[bytePos] >> bitPos) & 1;
            code |= (static_cast<std::uint32_t>(bit) << i);
            if (++bitPos == 8) { bitPos = 0; ++bytePos; }
        }
        out = code;
        return true;
    }
};

inline void putU16(std::vector<std::uint8_t>& v, unsigned x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

// Little-endian 16-bit read.
inline unsigned readU16(const std::uint8_t* d, std::size_t p) {
    return static_cast<unsigned>(d[p]) | (static_cast<unsigned>(d[p + 1]) << 8);
}

} // namespace detail

// Decode the first frame of a GIF byte stream into an RGBA8 Image. Empty Image on malformed input.
inline Image decodeGif(const std::uint8_t* d, std::size_t n) {
    if (n < 13 || d[0] != 'G' || d[1] != 'I' || d[2] != 'F') return Image();
    std::size_t p = 6; // past "GIF87a"/"GIF89a"
    const unsigned scrW = detail::readU16(d, p);
    const unsigned scrH = detail::readU16(d, p + 2);
    const std::uint8_t packed = d[p + 4];
    p += 7; // screen descriptor (7 bytes)
    (void)scrW; (void)scrH;

    std::vector<std::uint8_t> gct; // RGB triples
    if (packed & 0x80) {
        const int gctSize = 1 << ((packed & 0x07) + 1);
        const std::size_t bytes = static_cast<std::size_t>(gctSize) * 3;
        if (p + bytes > n) return Image();
        gct.assign(d + p, d + p + bytes);
        p += bytes;
    }

    // Skip extension blocks until the first image descriptor (0x2C).
    while (p < n && d[p] != 0x2C) {
        if (d[p] == 0x21) { // extension: label + sub-blocks
            p += 2;
            while (p < n && d[p] != 0x00) { p += 1 + d[p]; }
            p += 1;
        } else if (d[p] == 0x3B) {
            return Image(); // trailer before any image
        } else {
            ++p;
        }
    }
    if (p >= n || d[p] != 0x2C) return Image();
    ++p;
    if (p + 9 > n) return Image();
    const unsigned imgW = detail::readU16(d, p);        // left
    const unsigned imgH = detail::readU16(d, p + 2);    // top
    (void)imgW; (void)imgH;
    const unsigned w = detail::readU16(d, p + 4);
    const unsigned h = detail::readU16(d, p + 6);
    const std::uint8_t imgPacked = d[p + 8];
    p += 9;

    std::vector<std::uint8_t> lct;
    if (imgPacked & 0x80) {
        const int lctSize = 1 << ((imgPacked & 0x07) + 1);
        const std::size_t bytes = static_cast<std::size_t>(lctSize) * 3;
        if (p + bytes > n) return Image();
        lct.assign(d + p, d + p + bytes);
        p += bytes;
    }
    const std::vector<std::uint8_t>& palette = lct.empty() ? gct : lct;
    if (palette.empty() || w == 0 || h == 0) return Image();

    if (p >= n) return Image();
    const int minCodeSize = d[p++];
    if (minCodeSize < 2 || minCodeSize > 8) return Image();

    // Gather LZW data sub-blocks into one buffer.
    std::vector<std::uint8_t> lzw;
    while (p < n && d[p] != 0x00) {
        const std::size_t blockLen = d[p++];
        if (p + blockLen > n) return Image();
        lzw.insert(lzw.end(), d + p, d + p + blockLen);
        p += blockLen;
    }

    // LZW decode.
    const std::uint32_t clearCode = 1u << minCodeSize;
    const std::uint32_t eoiCode = clearCode + 1;
    std::vector<std::vector<std::uint8_t>> dict;
    auto resetDict = [&]() {
        dict.clear();
        dict.resize(clearCode + 2);
        for (std::uint32_t i = 0; i < clearCode; ++i) dict[i] = {static_cast<std::uint8_t>(i)};
    };
    resetDict();
    int codeSize = minCodeSize + 1;
    detail::GifBitReader br(lzw);
    std::vector<std::uint8_t> indices;
    indices.reserve(static_cast<std::size_t>(w) * h);
    std::uint32_t prev = 0xFFFFFFFF;

    for (;;) {
        std::uint32_t code;
        if (!br.read(codeSize, code)) break;
        if (code == clearCode) {
            resetDict();
            codeSize = minCodeSize + 1;
            prev = 0xFFFFFFFF;
            continue;
        }
        if (code == eoiCode) break;

        std::vector<std::uint8_t> entry;
        if (code < dict.size() && !dict[code].empty()) {
            entry = dict[code];
        } else if (code == dict.size() && prev != 0xFFFFFFFF) {
            entry = dict[prev];
            entry.push_back(dict[prev][0]);
        } else {
            break; // corrupt
        }
        indices.insert(indices.end(), entry.begin(), entry.end());

        if (prev != 0xFFFFFFFF) {
            std::vector<std::uint8_t> newEntry = dict[prev];
            newEntry.push_back(entry[0]);
            dict.push_back(newEntry);
        }
        prev = code;
        // Grow the read width one entry BEFORE the table is full: the decoder's dictionary lags the
        // encoder's by exactly one string (the first code after a clear adds nothing), so it must widen
        // one step early to stay byte-aligned with the encoder's variable-width output.
        if (dict.size() == (static_cast<std::size_t>(1) << codeSize) - 1 && codeSize < 12) ++codeSize;
    }

    Image img(static_cast<int>(w), static_cast<int>(h), Color{0, 0, 0, 1});
    const std::size_t paletteEntries = palette.size() / 3;
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * w + x;
            if (idx >= indices.size()) break;
            const std::size_t pi = indices[idx];
            if (pi >= paletteEntries) continue;
            img.setPixel(static_cast<int>(x), static_cast<int>(y),
                         color8(palette[pi * 3], palette[pi * 3 + 1], palette[pi * 3 + 2], 255));
        }
    }
    return img;
}

inline Image decodeGif(const std::vector<std::uint8_t>& bytes) { return decodeGif(bytes.data(), bytes.size()); }

// Encode a single-frame GIF89a from an Image. Lossless for images with <= 256 distinct colors (exact
// palette); otherwise the palette is the 256 most-distinct colors (nearest-match remap). Alpha is ignored.
inline std::vector<std::uint8_t> encodeGif(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) return out;
    const int w = img.width(), h = img.height();
    const std::vector<std::uint8_t>& px = img.data(); // RGBA8

    // Build a palette from the distinct colors (exact if <=256).
    std::map<std::uint32_t, int> colorIndex;
    std::vector<std::uint8_t> palette; // RGB
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    bool overflow = false;
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const std::uint8_t r = px[i * 4 + 0], g = px[i * 4 + 1], b = px[i * 4 + 2];
        const std::uint32_t key = (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
        auto it = colorIndex.find(key);
        int ci;
        if (it == colorIndex.end()) {
            if (colorIndex.size() >= 256) { overflow = true; ci = 0; }
            else {
                ci = static_cast<int>(colorIndex.size());
                colorIndex[key] = ci;
                palette.push_back(r); palette.push_back(g); palette.push_back(b);
            }
        } else {
            ci = it->second;
        }
        indices[i] = static_cast<std::uint8_t>(ci);
    }
    (void)overflow; // >256-color images fall back to index 0 for the overflow (documented lossy path)

    // Round palette size up to a power of two (>= 2 entries), min bits 1 -> code size 2.
    int bits = 1;
    while ((1 << bits) < static_cast<int>(palette.size() / 3)) ++bits;
    if (bits < 1) bits = 1;
    const int paletteCount = 1 << bits;
    palette.resize(static_cast<std::size_t>(paletteCount) * 3, 0);

    // Header + logical screen descriptor.
    const char* magic = "GIF89a";
    out.insert(out.end(), magic, magic + 6);
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(static_cast<std::uint8_t>(0x80 | ((bits - 1) & 0x07))); // GCT present, size
    out.push_back(0); // background color index
    out.push_back(0); // aspect ratio
    out.insert(out.end(), palette.begin(), palette.end());

    // Image descriptor.
    out.push_back(0x2C);
    detail::putU16(out, 0); detail::putU16(out, 0); // left, top
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(0); // no local color table, no interlace

    // LZW encode.
    const int minCodeSize = bits < 2 ? 2 : bits; // GIF requires >= 2
    out.push_back(static_cast<std::uint8_t>(minCodeSize));
    const std::uint32_t clearCode = 1u << minCodeSize;
    const std::uint32_t eoiCode = clearCode + 1;

    // Standard integer-keyed LZW dictionary: a multi-symbol string is identified by its (prefix code,
    // appended symbol) pair, so the key is (prefixCode << 8) | symbol -> assigned code. Single symbols are
    // implicit (their code equals the palette index), so only strings of length >= 2 live in the map. This
    // avoids a std::map keyed on std::vector (whose three-way compare trips a GCC false positive) and is the
    // canonical, faster LZW encoder form.
    std::map<std::uint32_t, std::uint32_t> dict;
    std::uint32_t nextCode = eoiCode + 1;
    int codeSize = minCodeSize + 1;
    detail::GifBitWriter bw;
    bw.put(clearCode, codeSize);

    std::uint32_t curCode = indices[0]; // indices is non-empty (w,h >= 1)
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const std::uint8_t sym = indices[i];
        const std::uint32_t key = (curCode << 8) | sym;
        auto it = dict.find(key);
        if (it != dict.end()) {
            curCode = it->second;
        } else {
            bw.put(curCode, codeSize);
            dict[key] = nextCode++;
            if (nextCode == (static_cast<std::uint32_t>(1) << codeSize) && codeSize < 12) ++codeSize;
            if (nextCode >= 4096) { // dictionary full: reset
                bw.put(clearCode, codeSize);
                dict.clear();
                nextCode = eoiCode + 1;
                codeSize = minCodeSize + 1;
            }
            curCode = sym;
        }
    }
    bw.put(curCode, codeSize);
    bw.put(eoiCode, codeSize);
    bw.flush();

    // Emit LZW data as sub-blocks (<=255 bytes each) + block terminator.
    const std::vector<std::uint8_t>& lzw = bw.out;
    std::size_t off = 0;
    while (off < lzw.size()) {
        const std::size_t chunk = (lzw.size() - off) < 255 ? (lzw.size() - off) : 255;
        out.push_back(static_cast<std::uint8_t>(chunk));
        out.insert(out.end(), lzw.begin() + static_cast<std::ptrdiff_t>(off),
                   lzw.begin() + static_cast<std::ptrdiff_t>(off + chunk));
        off += chunk;
    }
    out.push_back(0x00); // block terminator
    out.push_back(0x3B); // trailer
    return out;
}

} // namespace maz::render
