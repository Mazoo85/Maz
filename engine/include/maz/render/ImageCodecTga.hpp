#pragma once

#include "maz/render/Image.hpp" // Image, color8

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render TGA codec — a headless, dependency-free encoder/decoder between render::Image and the
// Truevision TGA (.tga) byte format Godot's Image can import. Unlike the runtime stb_image path
// (which needs the GPU texture upload), this works purely on CPU bytes: encode an Image you built
// procedurally to a .tga blob, or decode a .tga blob back into an editable Image. Uncompressed
// true-colour (type 2), 24- or 32-bit; encoding always writes 32-bit BGRA with a top-left origin.
namespace maz::render {

// Encode an Image to an uncompressed 32-bit TGA byte blob. Empty image -> empty vector.
inline std::vector<std::uint8_t> encodeTga(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) {
        return out;
    }
    const int w = img.width();
    const int h = img.height();
    out.reserve(18u + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
    auto push16 = [&out](int v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    };
    out.push_back(0); // ID length
    out.push_back(0); // colour-map type (none)
    out.push_back(2); // image type: uncompressed true-colour
    for (int i = 0; i < 5; ++i) {
        out.push_back(0); // colour-map specification (unused)
    }
    push16(0); // x-origin
    push16(0); // y-origin
    push16(w);
    push16(h);
    out.push_back(32);   // bits per pixel
    out.push_back(0x28); // descriptor: 8 alpha bits (0x08) + top-left origin (0x20)
    const std::vector<std::uint8_t>& px = img.data(); // RGBA8, top-left origin
    for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
        out.push_back(px[i + 2]); // B
        out.push_back(px[i + 1]); // G
        out.push_back(px[i]);     // R
        out.push_back(px[i + 3]); // A
    }
    return out;
}

// Decode an uncompressed true-colour TGA blob (24- or 32-bit) into an Image. Returns an empty Image
// on any malformed / unsupported input (colour-mapped, RLE, truncated, zero size).
inline Image decodeTga(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 18u) {
        return Image{};
    }
    const std::uint8_t idLen = data[0];
    const std::uint8_t cmapType = data[1];
    const std::uint8_t imgType = data[2];
    if (cmapType != 0 || imgType != 2) {
        return Image{}; // only uncompressed true-colour, no colour map
    }
    const int w = data[12] | (data[13] << 8);
    const int h = data[14] | (data[15] << 8);
    const std::uint8_t bpp = data[16];
    const std::uint8_t desc = data[17];
    if ((bpp != 32 && bpp != 24) || w <= 0 || h <= 0) {
        return Image{};
    }
    const int bytesPP = bpp / 8;
    const std::size_t offset = 18u + idLen; // colour map is 0 bytes (cmapType == 0)
    const std::size_t need =
        offset + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(bytesPP);
    if (size < need) {
        return Image{};
    }
    const bool topDown = (desc & 0x20) != 0;
    Image img(w, h);
    for (int row = 0; row < h; ++row) {
        const int y = topDown ? row : (h - 1 - row);
        for (int x = 0; x < w; ++x) {
            const std::size_t p =
                offset + (static_cast<std::size_t>(row) * static_cast<std::size_t>(w) +
                          static_cast<std::size_t>(x)) *
                             static_cast<std::size_t>(bytesPP);
            const int b = data[p];
            const int g = data[p + 1];
            const int r = data[p + 2];
            const int a = (bytesPP == 4) ? data[p + 3] : 255;
            img.setPixel(x, y, color8(r, g, b, a));
        }
    }
    return img;
}

// Convenience overload for a byte vector.
inline Image decodeTga(const std::vector<std::uint8_t>& bytes) {
    return decodeTga(bytes.data(), bytes.size());
}

} // namespace maz::render
