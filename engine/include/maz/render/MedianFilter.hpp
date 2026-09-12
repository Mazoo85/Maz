#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render median filter — remove "salt-and-pepper" speckle while keeping edges crisp.
//
// Replacing each pixel with the MEDIAN of its neighbourhood (rather than the average) is the classic
// impulse-noise cleaner: a lone bright or dark speckle is an outlier in the window, so the median simply
// ignores it — the pixel takes a neighbour's real value. Crucially, unlike a Gaussian/box blur, the
// median does NOT smear edges: on either side of a sharp boundary the majority of the window still holds
// that side's value, so the edge stays sharp. Used to clean noisy masks, denoise generated/scanned
// textures, and pre-filter before thresholding or edge detection. Clamp-to-edge borders, radius r gives
// a (2r+1)x(2r+1) window. Pure CPU, header-only, deterministic — unit-tested that it kills a speckle,
// leaves flat regions and sharp edges untouched, and matches a hand-computed window median.
namespace maz::render {

// Median-filter a grayscale byte image (`gray` row-major w*h). `radius` >= 1 sets the window size.
inline std::vector<std::uint8_t> medianFilter(const std::uint8_t* gray, int w, int h, int radius = 1) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(w < 0 || h < 0 ? 0 : w * h), 0);
    if (gray == nullptr || w <= 0 || h <= 0) return out;
    if (radius < 1) radius = 1;

    auto at = [&](int x, int y) -> std::uint8_t {
        if (x < 0) x = 0;
        if (x >= w) x = w - 1;
        if (y < 0) y = 0;
        if (y >= h) y = h - 1;
        return gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
    };

    std::vector<std::uint8_t> window;
    window.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            window.clear();
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    window.push_back(at(x + dx, y + dy));
                }
            }
            const std::size_t mid = window.size() / 2;
            std::nth_element(window.begin(), window.begin() + static_cast<std::ptrdiff_t>(mid),
                             window.end());
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] =
                window[mid];
        }
    }
    return out;
}

inline std::vector<std::uint8_t> medianFilter(const std::vector<std::uint8_t>& gray, int w, int h,
                                              int radius = 1) {
    return medianFilter(gray.data(), w, h, radius);
}

} // namespace maz::render
