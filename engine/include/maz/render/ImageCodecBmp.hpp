#pragma once

#include "maz/render/Image.hpp" // Image, color8

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render BMP codec — a headless, dependency-free encoder/decoder between render::Image and the
// uncompressed Windows BMP (.bmp) byte format Godot imports. Encodes 32-bit BGRA with a
// BITMAPINFOHEADER, bottom-up rows (the BMP default). Decodes uncompressed 24- or 32-bit BMPs,
// honouring the sign of the height field (bottom-up vs top-down) and the 4-byte row padding that
// 24-bit rows require. Pure CPU bytes, no GPU upload.
namespace maz::render {

// Encode an Image to an uncompressed 32-bit BMP byte blob. Empty image -> empty vector.
inline std::vector<std::uint8_t> encodeBmp(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) {
        return out;
    }
    const int w = img.width();
    const int h = img.height();
    const std::uint32_t pixelBytes =
        static_cast<std::uint32_t>(w) * static_cast<std::uint32_t>(h) * 4u;
    const std::uint32_t dataOffset = 54u; // 14-byte file header + 40-byte DIB header
    const std::uint32_t fileSize = dataOffset + pixelBytes;
    out.reserve(fileSize);
    auto push16 = [&out](std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    };
    auto push32 = [&out](std::uint32_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    };
    // BITMAPFILEHEADER
    out.push_back('B');
    out.push_back('M');
    push32(fileSize);
    push32(0); // reserved
    push32(dataOffset);
    // BITMAPINFOHEADER
    push32(40);
    push32(static_cast<std::uint32_t>(w));
    push32(static_cast<std::uint32_t>(h)); // positive -> bottom-up
    push16(1);                             // planes
    push16(32);                            // bpp
    push32(0);                             // BI_RGB (uncompressed)
    push32(pixelBytes);
    push32(2835); // x pixels-per-metre (~72 dpi)
    push32(2835); // y pixels-per-metre
    push32(0);    // colours used
    push32(0);    // important colours
    // Pixel data, bottom-up, BGRA (32-bit rows need no padding).
    for (int row = 0; row < h; ++row) {
        const int y = h - 1 - row; // file row 0 is the bottom image row
        for (int x = 0; x < w; ++x) {
            const Color c = img.getPixel(x, y);
            out.push_back(static_cast<std::uint8_t>(b8(c)));
            out.push_back(static_cast<std::uint8_t>(g8(c)));
            out.push_back(static_cast<std::uint8_t>(r8(c)));
            out.push_back(static_cast<std::uint8_t>(a8(c)));
        }
    }
    return out;
}

// Decode an uncompressed 24- or 32-bit BMP blob into an Image. Returns an empty Image on any
// malformed / unsupported input (bad magic, compressed, palettised, truncated).
inline Image decodeBmp(const std::uint8_t* d, std::size_t size) {
    if (d == nullptr || size < 54) {
        return Image{};
    }
    if (!(d[0] == 'B' && d[1] == 'M')) {
        return Image{};
    }
    auto rd32 = [d](std::size_t o) {
        return static_cast<std::uint32_t>(d[o]) | (static_cast<std::uint32_t>(d[o + 1]) << 8) |
               (static_cast<std::uint32_t>(d[o + 2]) << 16) |
               (static_cast<std::uint32_t>(d[o + 3]) << 24);
    };
    auto rd16 = [d](std::size_t o) {
        return static_cast<std::uint16_t>(d[o] | (d[o + 1] << 8));
    };
    const std::uint32_t dataOffset = rd32(10);
    const std::uint32_t dibSize = rd32(14);
    if (dibSize < 40) {
        return Image{}; // only BITMAPINFOHEADER (or larger) supported
    }
    const std::int32_t wSigned = static_cast<std::int32_t>(rd32(18));
    const std::int32_t hSigned = static_cast<std::int32_t>(rd32(22));
    const std::uint16_t bpp = rd16(28);
    const std::uint32_t compression = rd32(30);
    if (compression != 0 || (bpp != 24 && bpp != 32) || wSigned <= 0 || hSigned == 0) {
        return Image{};
    }
    const int w = wSigned;
    const bool topDown = hSigned < 0;
    const int h = topDown ? -hSigned : hSigned;
    const int bytesPP = bpp / 8;
    const std::size_t rowStride =
        ((static_cast<std::size_t>(w) * static_cast<std::size_t>(bytesPP) + 3u) / 4u) * 4u;
    const std::size_t need = dataOffset + rowStride * static_cast<std::size_t>(h);
    if (size < need) {
        return Image{};
    }
    Image img(w, h);
    for (int row = 0; row < h; ++row) {
        const int y = topDown ? row : (h - 1 - row);
        const std::size_t rowStart = dataOffset + static_cast<std::size_t>(row) * rowStride;
        for (int x = 0; x < w; ++x) {
            const std::size_t p = rowStart + static_cast<std::size_t>(x) * static_cast<std::size_t>(bytesPP);
            const int b = d[p];
            const int g = d[p + 1];
            const int r = d[p + 2];
            const int a = (bytesPP == 4) ? d[p + 3] : 255;
            img.setPixel(x, y, color8(r, g, b, a));
        }
    }
    return img;
}

// Convenience overload for a byte vector.
inline Image decodeBmp(const std::vector<std::uint8_t>& bytes) {
    return decodeBmp(bytes.data(), bytes.size());
}

} // namespace maz::render
