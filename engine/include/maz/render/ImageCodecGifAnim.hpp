#pragma once

#include "maz/render/ColorQuantize.hpp"
#include "maz/render/Image.hpp"
#include "maz/render/ImageCodecGif.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render animated GIF encoder — the engine's only way to write a PLAYABLE moving picture.
//
// Why this exists: the engine can draw frames and write them out one by one, but a folder of stills is
// not something anyone can watch. It has a video/Ivf.hpp DEMUXER and no codec, and writing a VP9 or
// H.264 encoder is a year's work rather than a feature. An animated GIF is the one moving format whose
// whole compressed form is LZW over palette indices -- which this engine already had, in the
// single-frame encoder next door -- so it is the one that can be written honestly in a header.
//
// Honest scope and cost, so nobody is surprised:
//   * 256 colours for the WHOLE animation, from one global palette. That is GIF, not a shortcut. A
//     dark, gradient-heavy source needs the dithering below or it bands visibly.
//   * No interframe compression. Every frame stores every pixel, so the file grows linearly with
//     length. Short loops, not features.
//   * Delays are in hundredths of a second, and most players silently clamp anything under ~2.
namespace maz::render {

struct GifAnimOptions {
    int delayCentiseconds = 8; // ~12.5 fps. Below 2 most players override it.
    int loopCount = 0;         // 0 = forever
    int maxColors = 256;
    bool dither = true;        // ordered dither before the palette lookup; hides GIF's banding
    int sampleStride = 7;      // pixels stepped when gathering colours for the palette
};

namespace detail {

// A 4x4 ordered-dither offset, in palette-step units. Plain nearest-colour mapping of a dark gradient
// to 256 colours produces visible bands; nudging each pixel by a small amount that varies across a 4x4
// tile breaks the bands into a texture the eye reads as the gradient.
inline int ditherOffset(int x, int y) {
    static const int kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return kBayer[static_cast<std::size_t>((y & 3) * 4 + (x & 3))] - 8; // -8..+7
}

inline std::uint8_t clampByteInt(int v) {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

} // namespace detail

// Encode `frames` (all the same size) as one animated GIF89a. Returns empty on bad input.
inline std::vector<std::uint8_t> encodeGifAnimation(const std::vector<Image>& frames,
                                                    const GifAnimOptions& opts = GifAnimOptions{}) {
    std::vector<std::uint8_t> out;
    if (frames.empty() || frames[0].empty()) {
        return out;
    }
    const int w = frames[0].width();
    const int h = frames[0].height();
    for (const Image& f : frames) {
        if (f.width() != w || f.height() != h) {
            return out; // a GIF has one logical screen; mismatched frames are a caller error
        }
    }

    // --- one palette for the whole animation ---
    //
    // Per-frame local palettes would look better and would also make every frame's colours drift,
    // which reads as the whole picture shimmering. One global palette is stabler and smaller.
    std::vector<Rgb8> sample;
    const std::size_t stride = static_cast<std::size_t>(opts.sampleStride < 1 ? 1 : opts.sampleStride);
    for (const Image& f : frames) {
        const std::vector<std::uint8_t>& px = f.data();
        for (std::size_t i = 0; i + 3 < px.size(); i += 4u * stride) {
            sample.push_back(Rgb8{px[i], px[i + 1], px[i + 2]});
        }
    }
    const int wanted = opts.maxColors < 2 ? 2 : (opts.maxColors > 256 ? 256 : opts.maxColors);
    std::vector<Rgb8> palette = quantizePalette(sample, static_cast<std::size_t>(wanted));
    if (palette.empty()) {
        palette.push_back(Rgb8{0, 0, 0});
    }
    const std::vector<std::uint8_t> cube = detail::buildPaletteCube(palette);

    int bits = 1;
    while ((1 << bits) < static_cast<int>(palette.size())) {
        ++bits;
    }
    const int paletteCount = 1 << bits;
    const int minCodeSize = bits < 2 ? 2 : bits;

    // --- header, logical screen, global colour table ---
    const char* magic = "GIF89a";
    out.insert(out.end(), magic, magic + 6);
    detail::putU16(out, static_cast<unsigned>(w));
    detail::putU16(out, static_cast<unsigned>(h));
    out.push_back(static_cast<std::uint8_t>(0x80 | ((bits - 1) & 0x07)));
    out.push_back(0); // background colour index
    out.push_back(0); // aspect ratio
    for (int i = 0; i < paletteCount; ++i) {
        const Rgb8 c = i < static_cast<int>(palette.size()) ? palette[static_cast<std::size_t>(i)]
                                                            : Rgb8{0, 0, 0};
        out.push_back(c.r);
        out.push_back(c.g);
        out.push_back(c.b);
    }

    // --- the Netscape application extension, which is what makes a GIF loop ---
    out.push_back(0x21);
    out.push_back(0xFF);
    out.push_back(0x0B);
    const char* nets = "NETSCAPE2.0";
    out.insert(out.end(), nets, nets + 11);
    out.push_back(0x03);
    out.push_back(0x01);
    detail::putU16(out, static_cast<unsigned>(opts.loopCount < 0 ? 0 : opts.loopCount));
    out.push_back(0x00);

    const int delay = opts.delayCentiseconds < 1 ? 1 : opts.delayCentiseconds;
    std::vector<std::uint8_t> indices(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

    for (const Image& f : frames) {
        // Graphic control extension: the delay, and "leave the frame there" as the disposal, since
        // every frame is complete.
        out.push_back(0x21);
        out.push_back(0xF9);
        out.push_back(0x04);
        out.push_back(0x04); // disposal 1 (do not dispose), no transparency
        detail::putU16(out, static_cast<unsigned>(delay));
        out.push_back(0x00); // transparent colour index (unused)
        out.push_back(0x00);

        // Image descriptor: full frame, no local colour table.
        out.push_back(0x2C);
        detail::putU16(out, 0);
        detail::putU16(out, 0);
        detail::putU16(out, static_cast<unsigned>(w));
        detail::putU16(out, static_cast<unsigned>(h));
        out.push_back(0x00);

        const std::vector<std::uint8_t>& px = f.data();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                       static_cast<std::size_t>(x)) * 4u;
                int r = px[i], g = px[i + 1], b = px[i + 2];
                if (opts.dither) {
                    const int d = detail::ditherOffset(x, y);
                    r = detail::clampByteInt(r + d);
                    g = detail::clampByteInt(g + d);
                    b = detail::clampByteInt(b + d);
                }
                indices[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                        static_cast<std::size_t>(x)] =
                    cube[(static_cast<std::size_t>(r >> 3) * 32u +
                          static_cast<std::size_t>(g >> 3)) * 32u +
                         static_cast<std::size_t>(b >> 3)];
            }
        }

        out.push_back(static_cast<std::uint8_t>(minCodeSize));
        const std::vector<std::uint8_t> blocks = detail::gifLzwBlocks(indices, minCodeSize);
        out.insert(out.end(), blocks.begin(), blocks.end());
    }

    out.push_back(0x3B); // trailer
    return out;
}

} // namespace maz::render
