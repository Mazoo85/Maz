#pragma once

#include "maz/render/ColorQuantize.hpp"
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
        // Widen when the table has filled the current width. The decoder's dictionary lags the
        // encoder's by exactly one string (the first code after a clear adds nothing), and the encoder
        // widens once it has ASSIGNED the code 1<<codeSize -- so the decoder, one behind, widens when
        // its own next free code reaches 1<<codeSize. Getting this off by one costs nothing against
        // our own encoder and makes every other decoder on earth lose the stream; see the reference
        // fixture in tests/render/gif.cpp, which is a GIF this file did not write.
        if (dict.size() == (static_cast<std::size_t>(1) << codeSize) && codeSize < 12) ++codeSize;
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

// Encode a single-frame GIF89a from an Image. Lossless for images with <= 256 distinct colors (the
// palette is then exactly those colors, in first-seen order); beyond that the palette is quantised and
// every pixel is mapped to its nearest entry. Alpha is ignored. For something that moves, see
// ImageCodecGifAnim.hpp.
namespace detail {

// LZW-encode a stream of palette indices and emit it as GIF sub-blocks (each at most 255 bytes),
// terminated by the zero-length block. This is the whole compressed-data portion of one GIF image
// block, after its min-code-size byte.
//
// Shared by the single-frame and animated encoders on purpose: an LZW writer with a dictionary reset
// and a growing code size is precisely the sort of code that goes subtly wrong in a second copy.
// A 5-bits-per-channel lookup cube from an RGB value to its nearest palette entry.
//
// The reason for the cube: mapping a frame by searching the palette per pixel is 256 distance
// computations each, and a few hundred frames of a 480x270 animation is tens of billions of them. The
// cube pays 32768 searches once and then answers every pixel with an array index.
inline std::vector<std::uint8_t> buildPaletteCube(const std::vector<Rgb8>& palette) {
    std::vector<std::uint8_t> cube(32u * 32u * 32u);
    for (int r = 0; r < 32; ++r) {
        for (int g = 0; g < 32; ++g) {
            for (int b = 0; b < 32; ++b) {
                // The centre of the cell, so a cell is not biased toward its dark corner.
                const Rgb8 c{static_cast<std::uint8_t>(r * 8 + 4),
                             static_cast<std::uint8_t>(g * 8 + 4),
                             static_cast<std::uint8_t>(b * 8 + 4)};
                cube[(static_cast<std::size_t>(r) * 32u + static_cast<std::size_t>(g)) * 32u +
                     static_cast<std::size_t>(b)] =
                    static_cast<std::uint8_t>(nearestPaletteIndex(c, palette));
            }
        }
    }
    return cube;
}

inline std::vector<std::uint8_t> gifLzwBlocks(const std::vector<std::uint8_t>& indices,
                                              int minCodeSize) {
    std::vector<std::uint8_t> out;
    if (indices.empty()) {
        out.push_back(0x00);
        return out;
    }
    const std::uint32_t clearCode = 1u << minCodeSize;
    const std::uint32_t eoiCode = clearCode + 1;

    // Standard integer-keyed LZW dictionary: a multi-symbol string is identified by its (prefix code,
    // appended symbol) pair, so the key is (prefixCode << 8) | symbol -> assigned code. Single symbols
    // are implicit (their code equals the palette index), so only strings of length >= 2 live in the
    // map.
    std::map<std::uint32_t, std::uint32_t> dict;
    std::uint32_t nextCode = eoiCode + 1;
    int codeSize = minCodeSize + 1;
    GifBitWriter bw;
    bw.put(clearCode, codeSize);

    std::uint32_t curCode = indices[0];
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const std::uint8_t sym = indices[i];
        const std::uint32_t key = (curCode << 8) | sym;
        auto it = dict.find(key);
        if (it != dict.end()) {
            curCode = it->second;
        } else {
            bw.put(curCode, codeSize);
            dict[key] = nextCode++;
            // Widen only once the code just assigned has used up the current width. `>` and not `==`:
            // with `==` the next code goes out one bit wide while a conformant decoder is still
            // reading the old width, and the stream desynchronises from there to the end of the file.
            if (nextCode > (static_cast<std::uint32_t>(1) << codeSize) && codeSize < 12) {
                ++codeSize;
            }
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
    return out;
}

} // namespace detail

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
    // More than 256 distinct colours: there is no exact palette, so quantise and map every pixel to
    // its nearest entry. The old behaviour here was to send every colour past the 256th to palette
    // entry 0, which turns a photograph into a picture of entry 0 -- and the function's own comment
    // claimed a nearest-match remap, so the code was not doing what it said.
    if (overflow) {
        std::vector<Rgb8> sample;
        // Sample rather than sweep: a large image has far more pixels than the palette needs to see,
        // and quantizePalette is the expensive part.
        const std::size_t stride =
            indices.size() > 200000u ? indices.size() / 200000u : static_cast<std::size_t>(1);
        for (std::size_t i = 0; i < indices.size(); i += stride) {
            sample.push_back(Rgb8{px[i * 4 + 0], px[i * 4 + 1], px[i * 4 + 2]});
        }
        std::vector<Rgb8> chosen = quantizePalette(sample, 256u);
        if (chosen.empty()) {
            chosen.push_back(Rgb8{0, 0, 0});
        }
        const std::vector<std::uint8_t> cube = detail::buildPaletteCube(chosen);
        palette.clear();
        for (const Rgb8& c : chosen) {
            palette.push_back(c.r);
            palette.push_back(c.g);
            palette.push_back(c.b);
        }
        for (std::size_t i = 0; i < indices.size(); ++i) {
            indices[i] = cube[(static_cast<std::size_t>(px[i * 4 + 0] >> 3) * 32u +
                               static_cast<std::size_t>(px[i * 4 + 1] >> 3)) * 32u +
                              static_cast<std::size_t>(px[i * 4 + 2] >> 3)];
        }
    }

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

    // LZW encode. The encoder itself lives in detail::gifLzwBlocks so that the animated encoder can
    // use the same one -- an LZW writer is exactly the kind of thing that must not exist twice.
    const int minCodeSize = bits < 2 ? 2 : bits; // GIF requires >= 2
    out.push_back(static_cast<std::uint8_t>(minCodeSize));
    const std::vector<std::uint8_t> blocks = detail::gifLzwBlocks(indices, minCodeSize);
    out.insert(out.end(), blocks.begin(), blocks.end());

    out.push_back(0x3B); // trailer
    return out;
}

} // namespace maz::render
