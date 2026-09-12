#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render::thinZhangSuen — morphological thinning (Zhang-Suen 1984): reduce a filled binary shape to its
// one-pixel-wide SKELETON, the centerline that captures the shape's topology. Where dilate/erode grow or
// shrink a region, thinning peels a shape down to its bones without breaking it apart — turning a thick blob
// into a stick-figure medial axis. It's the standard tool for extracting road/river centerlines from a mask,
// stroke skeletons for handwriting or gesture analysis, path graphs from painted regions, and shape
// descriptors. The algorithm repeatedly deletes boundary pixels whose removal neither breaks connectivity
// nor shortens an endpoint, in two alternating sub-passes, until nothing more can be removed. Godot has
// erode/dilate but no thinning. Operates on a width*height grid of 0/1. Header-only, std-only, deterministic.
namespace maz::render {

namespace detail {
// Ordered 8-neighbourhood p2..p9 (N, NE, E, SE, S, SW, W, NW) of (x,y); out-of-bounds counts as 0.
inline void thinNeighbours(const std::vector<std::uint8_t>& g, int w, int h, int x, int y, int p[8]) {
    const int dxs[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int dys[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int i = 0; i < 8; ++i) {
        const int nx = x + dxs[i], ny = y + dys[i];
        p[i] = (nx < 0 || ny < 0 || nx >= w || ny >= h)
                   ? 0
                   : (g[static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) +
                       static_cast<std::size_t>(nx)] ? 1 : 0);
    }
}
} // namespace detail

// Thin a binary image (0 = background, non-zero = foreground) to its skeleton. Returns the 0/1 skeleton.
inline std::vector<std::uint8_t> thinZhangSuen(int width, int height, const std::vector<std::uint8_t>& image) {
    std::vector<std::uint8_t> g(image.size());
    if (width <= 0 || height <= 0 ||
        image.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return g;
    }
    for (std::size_t i = 0; i < image.size(); ++i) {
        g[i] = image[i] ? 1u : 0u;
    }
    const int w = width, h = height;
    std::vector<std::size_t> toDelete;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int step = 0; step < 2; ++step) {
            toDelete.clear();
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const std::size_t idx =
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
                    if (!g[idx]) {
                        continue;
                    }
                    int p[8];
                    detail::thinNeighbours(g, w, h, x, y, p);
                    int bsum = 0;
                    for (int i = 0; i < 8; ++i) {
                        bsum += p[i];
                    }
                    if (bsum < 2 || bsum > 6) {
                        continue;
                    }
                    int a = 0; // 0->1 transitions around p2..p9,p2
                    for (int i = 0; i < 8; ++i) {
                        if (p[i] == 0 && p[(i + 1) % 8] == 1) {
                            ++a;
                        }
                    }
                    if (a != 1) {
                        continue;
                    }
                    // p[0]=p2(N) p[2]=p4(E) p[4]=p6(S) p[6]=p8(W)
                    if (step == 0) {
                        if (p[0] * p[2] * p[4] != 0) continue;
                        if (p[2] * p[4] * p[6] != 0) continue;
                    } else {
                        if (p[0] * p[2] * p[6] != 0) continue;
                        if (p[0] * p[4] * p[6] != 0) continue;
                    }
                    toDelete.push_back(idx);
                }
            }
            if (!toDelete.empty()) {
                changed = true;
                for (std::size_t idx : toDelete) {
                    g[idx] = 0;
                }
            }
        }
    }
    return g;
}

} // namespace maz::render
