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

namespace detail {

// One frame reduced to GIF's wire form: a power-of-two RGB palette, the palette bit-depth, and the
// LZW-compressed index stream (pre-sub-blocking). Shared by the single-frame and animated encoders so
// the palette-build + LZW is written once. Lossless for <= 256 distinct colours (exact palette);
// otherwise colours past 256 fall back to index 0 (the documented lossy path). Alpha is ignored.
struct GifFrameData {
    std::vector<std::uint8_t> palette; // RGB, resized to (1<<bits) entries
    int bits = 1;
    std::vector<std::uint8_t> lzw; // compressed index bytes (before 255-byte sub-blocking)
};

inline GifFrameData gifQuantizeAndCompress(const Image& img) {
    GifFrameData f;
    const int w = img.width(), h = img.height();
    const std::vector<std::uint8_t>& px = img.data(); // RGBA8

    std::map<std::uint32_t, int> colorIndex;
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (std::size_t i = 0; i < indices.size(); ++i) {
        const std::uint8_t r = px[i * 4 + 0], g = px[i * 4 + 1], b = px[i * 4 + 2];
        const std::uint32_t key =
            (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
        auto it = colorIndex.find(key);
        int ci;
        if (it == colorIndex.end()) {
            if (colorIndex.size() >= 256) {
                ci = 0; // overflow: documented lossy fallback
            } else {
                ci = static_cast<int>(colorIndex.size());
                colorIndex[key] = ci;
                f.palette.push_back(r);
                f.palette.push_back(g);
                f.palette.push_back(b);
            }
        } else {
            ci = it->second;
        }
        indices[i] = static_cast<std::uint8_t>(ci);
    }

    // Round palette size up to a power of two (>= 2 entries), min bits 1 -> code size 2.
    f.bits = 1;
    while ((1 << f.bits) < static_cast<int>(f.palette.size() / 3)) ++f.bits;
    const int paletteCount = 1 << f.bits;
    f.palette.resize(static_cast<std::size_t>(paletteCount) * 3, 0);

    // LZW encode. Integer-keyed dictionary: a multi-symbol string is identified by its (prefix code,
    // appended symbol) pair -> (prefixCode << 8) | symbol. Single symbols are implicit (code == palette
    // index), so only strings of length >= 2 live in the map (avoids a std::map keyed on std::vector,
    // whose three-way compare trips a GCC false positive, and is the canonical faster form).
    const int minCodeSize = f.bits < 2 ? 2 : f.bits; // GIF requires >= 2
    const std::uint32_t clearCode = 1u << minCodeSize;
    const std::uint32_t eoiCode = clearCode + 1;
    std::map<std::uint32_t, std::uint32_t> dict;
    std::uint32_t nextCode = eoiCode + 1;
    int codeSize = minCodeSize + 1;
    GifBitWriter bw;
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
    f.lzw = std::move(bw.out);
    return f;
}

// Emit an LZW byte stream as GIF data sub-blocks (<= 255 bytes each) followed by the block terminator.
inline void writeGifSubBlocks(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& lzw) {
    std::size_t off = 0;
    while (off < lzw.size()) {
        const std::size_t chunk = (lzw.size() - off) < 255 ? (lzw.size() - off) : 255;
        out.push_back(static_cast<std::uint8_t>(chunk));
        out.insert(out.end(), lzw.begin() + static_cast<std::ptrdiff_t>(off),
                   lzw.begin() + static_cast<std::ptrdiff_t>(off + chunk));
        off += chunk;
    }
    out.push_back(0x00); // block terminator
}

} // namespace detail

// Encode a single-frame GIF89a from an Image. Lossless for images with <= 256 distinct colors (exact
// palette); otherwise colours past 256 fall back to index 0. Alpha is ignored.
inline std::vector<std::uint8_t> encodeGif(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) return out;
    const int w = img.width(), h = img.height();
    const detail::GifFrameData f = detail::gifQuantizeAndCompress(img);

    // Header + logical screen descriptor (global color table = this frame's palette).
    const char* magic = "GIF89a";
    out.insert(out.end(), magic, magic + 6);
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(static_cast<std::uint8_t>(0x80 | ((f.bits - 1) & 0x07))); // GCT present, size
    out.push_back(0);                                                       // background color index
    out.push_back(0);                                                       // aspect ratio
    out.insert(out.end(), f.palette.begin(), f.palette.end());

    // Image descriptor (no local color table, no interlace).
    out.push_back(0x2C);
    detail::putU16(out, 0);
    detail::putU16(out, 0); // left, top
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(0);

    out.push_back(static_cast<std::uint8_t>(f.bits < 2 ? 2 : f.bits)); // min code size
    detail::writeGifSubBlocks(out, f.lzw);
    out.push_back(0x3B); // trailer
    return out;
}

// Encode a multi-frame (animated) GIF89a from a list of equally-sized frames. Each frame carries its
// OWN local color table (so frames need not share a palette) and a Graphic Control Extension giving its
// display time; a NETSCAPE2.0 application extension sets the loop count (0 = loop forever). This is the
// self-contained "video" deliverable for a rendered cutscene frame sequence — no external encoder.
//
// `delayCentiseconds` is the per-frame delay in 1/100 s (e.g. 4 ≈ 25 fps, 2 ≈ 50 fps; GIF's timing
// granularity is a centisecond). Frames whose dimensions differ from the first are skipped. Returns an
// empty buffer if there are no usable frames. Per-frame quantization is lossless for <= 256 colours.
inline std::vector<std::uint8_t> encodeGifAnimation(const std::vector<Image>& frames,
                                                    int delayCentiseconds, int loopCount = 0) {
    std::vector<std::uint8_t> out;
    if (frames.empty() || frames.front().empty()) {
        return out;
    }
    const int w = frames.front().width(), h = frames.front().height();
    const unsigned delay = delayCentiseconds < 0 ? 0u : static_cast<unsigned>(delayCentiseconds);
    const unsigned loops = loopCount < 0 ? 0u : static_cast<unsigned>(loopCount);

    // Header + logical screen descriptor (no global color table; each frame brings a local one).
    const char* magic = "GIF89a";
    out.insert(out.end(), magic, magic + 6);
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(0x00); // no GCT
    out.push_back(0);    // background color index
    out.push_back(0);    // aspect ratio

    // NETSCAPE2.0 looping application extension.
    const char* netscape = "NETSCAPE2.0";
    out.push_back(0x21);
    out.push_back(0xFF);
    out.push_back(0x0B);
    out.insert(out.end(), netscape, netscape + 11);
    out.push_back(0x03); // sub-block length
    out.push_back(0x01); // sub-block id
    detail::putU16(out, loops);
    out.push_back(0x00); // block terminator

    for (const Image& frame : frames) {
        if (frame.empty() || frame.width() != w || frame.height() != h) {
            continue; // only equally-sized frames
        }
        const detail::GifFrameData f = detail::gifQuantizeAndCompress(frame);

        // Graphic Control Extension: disposal 0, no transparency, this frame's delay.
        out.push_back(0x21);
        out.push_back(0xF9);
        out.push_back(0x04);
        out.push_back(0x00); // packed: no disposal / transparency
        detail::putU16(out, delay);
        out.push_back(0x00); // transparent color index (unused)
        out.push_back(0x00); // block terminator

        // Image descriptor with a local color table (0x80 | size bits).
        out.push_back(0x2C);
        detail::putU16(out, 0);
        detail::putU16(out, 0); // left, top
        detail::putU16(out, static_cast<unsigned>(w));
        detail::putU16(out, static_cast<unsigned>(h));
        out.push_back(static_cast<std::uint8_t>(0x80 | ((f.bits - 1) & 0x07)));
        out.insert(out.end(), f.palette.begin(), f.palette.end());

        out.push_back(static_cast<std::uint8_t>(f.bits < 2 ? 2 : f.bits)); // min code size
        detail::writeGifSubBlocks(out, f.lzw);
    }
    out.push_back(0x3B); // trailer
    return out;
}

} // namespace maz::render
