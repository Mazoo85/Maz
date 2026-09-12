#pragma once

#include "maz/render/Image.hpp" // Color

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::render median-cut colour quantization — shrink a full-colour image down to a small, representative
// PALETTE of at most K colours, the way a GIF, an indexed texture, or a deliberately retro/limited-palette
// look is produced. It recursively splits the cloud of pixel colours: at each step it takes the box of
// colours with the widest spread along red, green, or blue and cuts it at the MEDIAN of that channel, so
// dense regions of colour get more palette entries than sparse ones. Each final box contributes its average
// colour to the palette. This is the classic Heckbert median cut — better balanced than a naive "keep the
// most common colours" pass, which is what the GIF encoder currently falls back to. Pair it with
// `nearestColor` to remap the image to palette indices. Godot has no runtime colour quantizer. Header-only,
// std-only, deterministic.
namespace maz::render {

// Reduce `pixels` to at most `maxColors` representative colours via median cut (RGB only; alpha ignored).
inline std::vector<Color> medianCutPalette(const std::vector<Color>& pixels, int maxColors) {
    std::vector<Color> palette;
    if (pixels.empty() || maxColors < 1) {
        return palette;
    }
    auto channel = [&](const Color& c, int ch) { return ch == 0 ? c.r : (ch == 1 ? c.g : c.b); };

    std::vector<std::vector<int>> boxes;
    std::vector<int> all(pixels.size());
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        all[i] = static_cast<int>(i);
    }
    boxes.push_back(std::move(all));

    while (static_cast<int>(boxes.size()) < maxColors) {
        // Find the box + channel with the largest colour spread.
        int bestBox = -1, bestCh = 0;
        float bestExt = 0.0f;
        for (std::size_t bi = 0; bi < boxes.size(); ++bi) {
            if (boxes[bi].size() < 2) {
                continue;
            }
            for (int ch = 0; ch < 3; ++ch) {
                float mn = 1e30f, mx = -1e30f;
                for (int id : boxes[bi]) {
                    const float v = channel(pixels[static_cast<std::size_t>(id)], ch);
                    mn = std::min(mn, v);
                    mx = std::max(mx, v);
                }
                const float ext = mx - mn;
                if (ext > bestExt) {
                    bestExt = ext;
                    bestBox = static_cast<int>(bi);
                    bestCh = ch;
                }
            }
        }
        if (bestBox < 0 || bestExt <= 1e-9f) {
            break; // nothing left to split
        }
        std::vector<int>& box = boxes[static_cast<std::size_t>(bestBox)];
        std::sort(box.begin(), box.end(), [&](int a, int b) {
            return channel(pixels[static_cast<std::size_t>(a)], bestCh) <
                   channel(pixels[static_cast<std::size_t>(b)], bestCh);
        });
        const std::size_t mid = box.size() / 2;
        std::vector<int> right(box.begin() + static_cast<std::ptrdiff_t>(mid), box.end());
        box.resize(mid);
        boxes.push_back(std::move(right));
    }

    for (const std::vector<int>& box : boxes) {
        if (box.empty()) {
            continue;
        }
        float r = 0.0f, g = 0.0f, b = 0.0f;
        for (int id : box) {
            r += pixels[static_cast<std::size_t>(id)].r;
            g += pixels[static_cast<std::size_t>(id)].g;
            b += pixels[static_cast<std::size_t>(id)].b;
        }
        const float n = static_cast<float>(box.size());
        palette.push_back(Color{r / n, g / n, b / n, 1.0f});
    }
    return palette;
}

// Index of the palette colour nearest `c` by squared RGB distance (-1 if the palette is empty).
inline int nearestColor(const std::vector<Color>& palette, const Color& c) {
    int best = -1;
    float bestD = 1e30f;
    for (std::size_t i = 0; i < palette.size(); ++i) {
        const float dr = palette[i].r - c.r, dg = palette[i].g - c.g, db = palette[i].b - c.b;
        const float d = dr * dr + dg * dg + db * db;
        if (d < bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace maz::render
