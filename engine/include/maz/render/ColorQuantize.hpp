#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// maz::render color quantization — reduce an arbitrary set of RGB colors down to a small
// representative palette via the classic MEDIAN-CUT algorithm. Repeatedly split the color box with the
// widest channel spread at its median along that channel, then average each final box to a palette
// entry. The tool for retro/indexed-color looks (NES/GameBoy-style palettes), GIF-style export,
// texture palettization, and "dominant colors of this image" swatches — none of which Godot provides.
// Works in 0-255 RGB space; pair quantizePalette() with mapToPalette()/nearestPaletteIndex() to remap
// an image onto the reduced palette. Header-only, std-only, deterministic.
namespace maz::render {

struct Rgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

inline bool operator==(Rgb8 a, Rgb8 b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline bool operator!=(Rgb8 a, Rgb8 b) { return !(a == b); }

// Index of the palette entry closest (squared RGB distance) to color c. 0 for an empty palette.
inline std::size_t nearestPaletteIndex(Rgb8 c, const std::vector<Rgb8>& palette) {
    std::size_t best = 0;
    long bestDist = std::numeric_limits<long>::max();
    for (std::size_t i = 0; i < palette.size(); ++i) {
        const long dr = static_cast<long>(c.r) - palette[i].r;
        const long dg = static_cast<long>(c.g) - palette[i].g;
        const long db = static_cast<long>(c.b) - palette[i].b;
        const long d = dr * dr + dg * dg + db * db;
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

// Build a palette of at most maxColors representative colors from `pixels` (median cut).
inline std::vector<Rgb8> quantizePalette(const std::vector<Rgb8>& pixels, std::size_t maxColors) {
    if (pixels.empty() || maxColors == 0) {
        return {};
    }

    std::vector<std::vector<Rgb8>> boxes;
    boxes.push_back(pixels);

    while (boxes.size() < maxColors) {
        // Pick the box with the widest single-channel spread that can still be split.
        int bestBox = -1;
        int bestChannel = 0;
        int bestRange = 0;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            const std::vector<Rgb8>& box = boxes[i];
            if (box.size() < 2) {
                continue;
            }
            int mn[3] = {255, 255, 255};
            int mx[3] = {0, 0, 0};
            for (const Rgb8& c : box) {
                const int v[3] = {c.r, c.g, c.b};
                for (int ch = 0; ch < 3; ++ch) {
                    mn[ch] = std::min(mn[ch], v[ch]);
                    mx[ch] = std::max(mx[ch], v[ch]);
                }
            }
            for (int ch = 0; ch < 3; ++ch) {
                const int range = mx[ch] - mn[ch];
                if (range > bestRange) {
                    bestRange = range;
                    bestBox = static_cast<int>(i);
                    bestChannel = ch;
                }
            }
        }
        if (bestBox < 0 || bestRange <= 0) {
            break; // every box is a single color — nothing left to split
        }

        std::vector<Rgb8>& box = boxes[static_cast<std::size_t>(bestBox)];
        const int channel = bestChannel;
        std::sort(box.begin(), box.end(), [channel](const Rgb8& a, const Rgb8& b) {
            const int av = channel == 0 ? a.r : (channel == 1 ? a.g : a.b);
            const int bv = channel == 0 ? b.r : (channel == 1 ? b.g : b.b);
            return av < bv;
        });
        const std::size_t mid = box.size() / 2;
        std::vector<Rgb8> lo(box.begin(), box.begin() + static_cast<std::ptrdiff_t>(mid));
        std::vector<Rgb8> hi(box.begin() + static_cast<std::ptrdiff_t>(mid), box.end());
        boxes[static_cast<std::size_t>(bestBox)] = std::move(lo);
        boxes.push_back(std::move(hi));
    }

    std::vector<Rgb8> palette;
    palette.reserve(boxes.size());
    for (const std::vector<Rgb8>& box : boxes) {
        if (box.empty()) {
            continue;
        }
        std::uint64_t sr = 0;
        std::uint64_t sg = 0;
        std::uint64_t sb = 0;
        for (const Rgb8& c : box) {
            sr += c.r;
            sg += c.g;
            sb += c.b;
        }
        const std::uint64_t n = box.size();
        palette.push_back(Rgb8{static_cast<std::uint8_t>((sr + n / 2) / n),
                               static_cast<std::uint8_t>((sg + n / 2) / n),
                               static_cast<std::uint8_t>((sb + n / 2) / n)});
    }
    return palette;
}

// Remap each pixel to the index of its nearest palette entry.
inline std::vector<std::size_t> mapToPalette(const std::vector<Rgb8>& pixels,
                                             const std::vector<Rgb8>& palette) {
    std::vector<std::size_t> out;
    out.reserve(pixels.size());
    for (const Rgb8& c : pixels) {
        out.push_back(nearestPaletteIndex(c, palette));
    }
    return out;
}

} // namespace maz::render
