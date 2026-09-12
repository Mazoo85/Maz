#pragma once

#include "maz/render/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// maz::render Netpbm (PNM) image codec — decode/encode between render::Image and the PBM/PGM/PPM family
// (magic P1..P6). Netpbm is the simplest, most universal raster interchange: GIMP, ImageMagick, netpbm
// tools, scientific/CV pipelines and many render farms emit it, and it is trivially hand-writable, which
// makes it valuable both as an import format and as a debugging output. Decodes all six variants — ASCII
// bitmap/graymap/pixmap (P1/P2/P3) and binary bitmap/graymap/pixmap (P4/P5/P6) — into an RGBA8 Image;
// encodes to binary P6 (RGB) or ASCII P3. Pure CPU bytes (no GPU), so it unit-tests headlessly from an
// in-memory buffer.
//
// Scope note (honest): 8-bit maxval (the ubiquitous case) — a `maxval > 255` (16-bit) sample is not
// rescaled. Bitmaps map 1->black, 0->white (PBM convention). Alpha is always opaque (PNM has no alpha).
namespace maz::render {

namespace detail {

// Skip PNM whitespace and '#' comments, then read one unsigned integer token. Returns false at EOF/non-digit.
inline bool pnmReadUint(const std::uint8_t* d, std::size_t n, std::size_t& p, long& out) {
    for (;;) {
        while (p < n && (d[p] == ' ' || d[p] == '\t' || d[p] == '\n' || d[p] == '\r')) ++p;
        if (p < n && d[p] == '#') { while (p < n && d[p] != '\n') ++p; continue; }
        break;
    }
    if (p >= n || d[p] < '0' || d[p] > '9') return false;
    long v = 0;
    while (p < n && d[p] >= '0' && d[p] <= '9') { v = v * 10 + (d[p] - '0'); ++p; }
    out = v;
    return true;
}

// P1 (ASCII bitmap) reads single 0/1 characters that may not be whitespace-separated.
inline bool pnmReadBit(const std::uint8_t* d, std::size_t n, std::size_t& p, int& out) {
    for (;;) {
        while (p < n && (d[p] == ' ' || d[p] == '\t' || d[p] == '\n' || d[p] == '\r')) ++p;
        if (p < n && d[p] == '#') { while (p < n && d[p] != '\n') ++p; continue; }
        break;
    }
    if (p >= n || (d[p] != '0' && d[p] != '1')) return false;
    out = d[p] - '0';
    ++p;
    return true;
}

} // namespace detail

// Decode a PNM byte stream into an Image. Returns an empty Image on malformed / unsupported input.
inline Image decodePnm(const std::uint8_t* d, std::size_t n) {
    if (n < 2 || d[0] != 'P') return Image();
    const char type = static_cast<char>(d[1]);
    if (type < '1' || type > '6') return Image();
    std::size_t p = 2;

    long w = 0, h = 0, maxv = 1;
    if (!detail::pnmReadUint(d, n, p, w)) return Image();
    if (!detail::pnmReadUint(d, n, p, h)) return Image();
    const bool hasMax = (type != '1' && type != '4');
    if (hasMax) {
        if (!detail::pnmReadUint(d, n, p, maxv)) return Image();
    }
    if (w <= 0 || h <= 0 || (hasMax && (maxv <= 0 || maxv > 255))) return Image();

    Image img(static_cast<int>(w), static_cast<int>(h), Color{0.0f, 0.0f, 0.0f, 1.0f});
    const int iw = static_cast<int>(w), ih = static_cast<int>(h);

    auto put = [&](int x, int y, int r, int g, int b) {
        img.setPixel(x, y, color8(r, g, b, 255));
    };

    if (type == '1' || type == '2' || type == '3') {
        // ASCII bodies.
        for (int y = 0; y < ih; ++y) {
            for (int x = 0; x < iw; ++x) {
                if (type == '1') {
                    int bit = 0;
                    if (!detail::pnmReadBit(d, n, p, bit)) return Image();
                    const int v = bit ? 0 : 255;  // 1 = black
                    put(x, y, v, v, v);
                } else if (type == '2') {
                    long g = 0;
                    if (!detail::pnmReadUint(d, n, p, g)) return Image();
                    const int v = static_cast<int>(g);
                    put(x, y, v, v, v);
                } else {  // P3
                    long r = 0, gg = 0, b = 0;
                    if (!detail::pnmReadUint(d, n, p, r) || !detail::pnmReadUint(d, n, p, gg) ||
                        !detail::pnmReadUint(d, n, p, b))
                        return Image();
                    put(x, y, static_cast<int>(r), static_cast<int>(gg), static_cast<int>(b));
                }
            }
        }
        return img;
    }

    // Binary bodies (P4/P5/P6): exactly one whitespace char separates the header from the data.
    if (p < n && (d[p] == ' ' || d[p] == '\t' || d[p] == '\n' || d[p] == '\r')) ++p;

    if (type == '4') {  // packed bitmap: 8 pixels/byte, MSB first, rows byte-aligned
        const std::size_t rowBytes = static_cast<std::size_t>((iw + 7) / 8);
        if (p + rowBytes * static_cast<std::size_t>(ih) > n) return Image();
        for (int y = 0; y < ih; ++y) {
            const std::uint8_t* row = d + p + static_cast<std::size_t>(y) * rowBytes;
            for (int x = 0; x < iw; ++x) {
                const int bit = (row[x / 8] >> (7 - (x % 8))) & 1;
                const int v = bit ? 0 : 255;
                put(x, y, v, v, v);
            }
        }
        return img;
    }

    const std::size_t chans = (type == '6') ? 3u : 1u;
    const std::size_t need = static_cast<std::size_t>(iw) * static_cast<std::size_t>(ih) * chans;
    if (p + need > n) return Image();
    std::size_t c = p;
    for (int y = 0; y < ih; ++y) {
        for (int x = 0; x < iw; ++x) {
            if (type == '6') {
                const int r = d[c], g = d[c + 1], b = d[c + 2];
                c += 3;
                put(x, y, r, g, b);
            } else {  // P5
                const int v = d[c++];
                put(x, y, v, v, v);
            }
        }
    }
    return img;
}

inline Image decodePnm(const std::vector<std::uint8_t>& bytes) {
    return decodePnm(bytes.data(), bytes.size());
}

// Encode an Image to binary P6 (RGB pixmap). Alpha is dropped (PNM has no alpha).
inline std::vector<std::uint8_t> encodePnmP6(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) return out;
    const std::string header =
        "P6\n" + std::to_string(img.width()) + " " + std::to_string(img.height()) + "\n255\n";
    out.insert(out.end(), header.begin(), header.end());
    const std::vector<std::uint8_t>& px = img.data();  // RGBA8, row-major, top-left origin
    const std::size_t count = static_cast<std::size_t>(img.width()) * static_cast<std::size_t>(img.height());
    out.reserve(out.size() + count * 3);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(px[i * 4 + 0]);
        out.push_back(px[i * 4 + 1]);
        out.push_back(px[i * 4 + 2]);
    }
    return out;
}

// Encode an Image to ASCII P3 (RGB pixmap) — human-readable, handy for tests/debugging.
inline std::vector<std::uint8_t> encodePnmP3(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) return out;
    std::string s = "P3\n" + std::to_string(img.width()) + " " + std::to_string(img.height()) + "\n255\n";
    const std::vector<std::uint8_t>& px = img.data();
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width()) +
                                   static_cast<std::size_t>(x)) * 4;
            s += std::to_string(px[i]) + " " + std::to_string(px[i + 1]) + " " + std::to_string(px[i + 2]) + " ";
        }
        s += "\n";
    }
    out.assign(s.begin(), s.end());
    return out;
}

} // namespace maz::render
