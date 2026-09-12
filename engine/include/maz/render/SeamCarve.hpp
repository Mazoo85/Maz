#pragma once

#include "maz/render/Image.hpp" // Image, Color

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render::SeamCarve — content-aware image resizing (Avidan & Shamir 2007). Ordinary scaling squashes
// everything uniformly; seam carving instead removes the least-important pixels, so a photo can be narrowed
// while the interesting subject keeps its shape and only the bland background is squeezed out. It works by
// scoring every pixel with an "energy" (how much its colour differs from its neighbours — edges are high,
// flat regions low), then repeatedly deleting a *seam*: a connected, one-pixel-wide path from top to bottom
// (or left to right) that threads through the lowest total energy. The optimal seam is found by dynamic
// programming, and removing it shifts the rest of the row over, shrinking the image by one column at a time.
// Godot has no content-aware resize (Image.resize only interpolates); this is the real thing, exact on the
// CPU and fully testable — the chosen seam provably minimizes total energy, verifiable against brute force.
// Header-only, std-only, deterministic (ties break to the leftmost column).
namespace maz::render {

// Dual-gradient energy at (x,y): the colour gradient magnitude, using clamped (edge-extended) neighbours so
// border pixels are well defined. Higher = more of an edge / more visually important.
inline float seamEnergyAt(const Image& img, int x, int y) {
    const int w = img.width(), h = img.height();
    const int xl = x > 0 ? x - 1 : 0;
    const int xr = x + 1 < w ? x + 1 : w - 1;
    const int yu = y > 0 ? y - 1 : 0;
    const int yd = y + 1 < h ? y + 1 : h - 1;
    const Color cl = img.getPixel(xl, y), cr = img.getPixel(xr, y);
    const Color cu = img.getPixel(x, yu), cd = img.getPixel(x, yd);
    const float dxr = cr.r - cl.r, dxg = cr.g - cl.g, dxb = cr.b - cl.b;
    const float dyr = cd.r - cu.r, dyg = cd.g - cu.g, dyb = cd.b - cu.b;
    const float dx2 = dxr * dxr + dxg * dxg + dxb * dxb;
    const float dy2 = dyr * dyr + dyg * dyg + dyb * dyb;
    return std::sqrt(dx2 + dy2);
}

// Find the minimum-energy vertical seam: returns, for each row y (0..height-1), the column of the seam.
// Consecutive rows differ by at most one column (8-connected). Empty / width<2 -> empty seam.
inline std::vector<int> findVerticalSeam(const Image& img) {
    const int w = img.width(), h = img.height();
    std::vector<int> seam;
    if (w < 2 || h < 1) {
        return seam;
    }
    const std::size_t wz = static_cast<std::size_t>(w);
    // Precompute the energy grid, then DP the cumulative minimum energy top-to-bottom.
    std::vector<double> cost(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    std::vector<int> from(cost.size(), 0); // parent column chosen from the row above
    for (int x = 0; x < w; ++x) {
        cost[static_cast<std::size_t>(x)] = static_cast<double>(seamEnergyAt(img, x, 0));
    }
    for (int y = 1; y < h; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * wz;
        const std::size_t prev = static_cast<std::size_t>(y - 1) * wz;
        for (int x = 0; x < w; ++x) {
            int bestCol = x;
            double best = cost[prev + static_cast<std::size_t>(x)];
            if (x > 0 && cost[prev + static_cast<std::size_t>(x - 1)] < best) {
                best = cost[prev + static_cast<std::size_t>(x - 1)];
                bestCol = x - 1;
            }
            if (x + 1 < w && cost[prev + static_cast<std::size_t>(x + 1)] < best) {
                best = cost[prev + static_cast<std::size_t>(x + 1)];
                bestCol = x + 1;
            }
            cost[row + static_cast<std::size_t>(x)] = static_cast<double>(seamEnergyAt(img, x, y)) + best;
            from[row + static_cast<std::size_t>(x)] = bestCol;
        }
    }
    // Backtrack from the lowest-cost column of the last row (leftmost on ties).
    const std::size_t last = static_cast<std::size_t>(h - 1) * wz;
    int end = 0;
    double bestEnd = cost[last];
    for (int x = 1; x < w; ++x) {
        if (cost[last + static_cast<std::size_t>(x)] < bestEnd) {
            bestEnd = cost[last + static_cast<std::size_t>(x)];
            end = x;
        }
    }
    seam.assign(static_cast<std::size_t>(h), 0);
    int col = end;
    for (int y = h - 1; y >= 0; --y) {
        seam[static_cast<std::size_t>(y)] = col;
        col = from[static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(col)];
    }
    return seam;
}

// Remove a vertical seam, producing an image one column narrower. Each row drops its seam pixel and the
// pixels to the right shift left. If the seam is malformed/empty the source is returned unchanged.
inline Image removeVerticalSeam(const Image& img, const std::vector<int>& seam) {
    const int w = img.width(), h = img.height();
    if (w < 2 || seam.size() != static_cast<std::size_t>(h)) {
        return img;
    }
    Image out(w - 1, h);
    for (int y = 0; y < h; ++y) {
        const int cut = seam[static_cast<std::size_t>(y)];
        int dx = 0;
        for (int x = 0; x < w; ++x) {
            if (x == cut) {
                continue;
            }
            out.setPixel(dx, y, img.getPixel(x, y));
            ++dx;
        }
    }
    return out;
}

// Transpose (rows<->cols) so horizontal seams reuse the vertical machinery.
inline Image seamTranspose(const Image& img) {
    Image out(img.height(), img.width());
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            out.setPixel(y, x, img.getPixel(x, y));
        }
    }
    return out;
}

inline std::vector<int> findHorizontalSeam(const Image& img) { return findVerticalSeam(seamTranspose(img)); }

// Shrink width to `targetWidth` by removing the appropriate number of least-energy vertical seams.
// targetWidth >= width returns the image unchanged; it is clamped to at least 1.
inline Image carveWidth(const Image& img, int targetWidth) {
    if (targetWidth < 1) {
        targetWidth = 1;
    }
    Image cur = img;
    while (cur.width() > targetWidth && cur.width() >= 2) {
        cur = removeVerticalSeam(cur, findVerticalSeam(cur));
    }
    return cur;
}

// Shrink height to `targetHeight` by carving horizontal seams (via transpose).
inline Image carveHeight(const Image& img, int targetHeight) {
    return seamTranspose(carveWidth(seamTranspose(img), targetHeight));
}

} // namespace maz::render
