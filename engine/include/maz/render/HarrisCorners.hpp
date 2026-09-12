#pragma once

#include "maz/render/Image.hpp" // Image, Color

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::render::harrisCorners — the Harris & Stephens corner detector: find the distinctive, trackable
// "corner" points in an image (where brightness changes sharply in TWO directions), as opposed to flat
// regions (no change) or straight edges (change in only one direction). Corners are the stable landmarks
// used to align/stitch images, match features between frames, calibrate, auto-register decals or sprites,
// and drive simple optical-flow tracking. It works from the local structure tensor — sums of squared image
// gradients over a small window — whose two eigenvalues are both large only at a true corner; the Harris
// response det(M) - k*trace(M)^2 captures that without an eigen-solve. Non-maximum suppression then keeps
// only the strongest response in each neighbourhood. Godot ships no feature detector. Header-only, std-only,
// deterministic; operates on the luminance of a maz::render::Image.
namespace maz::render {

struct Corner {
    int x = 0;
    int y = 0;
    float response = 0.0f;
};

// Detect corners. `k` is the Harris sensitivity (~0.04-0.06); a point is kept if its response is a local
// maximum within `nmsRadius` and exceeds `relThreshold` times the strongest response in the image.
inline std::vector<Corner> harrisCorners(const Image& img, float k = 0.04f, float relThreshold = 0.01f,
                                         int nmsRadius = 2) {
    std::vector<Corner> corners;
    const int w = img.width(), h = img.height();
    if (w < 5 || h < 5) {
        return corners;
    }
    const std::size_t wz = static_cast<std::size_t>(w);
    // Luminance.
    std::vector<float> lum(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Color c = img.getPixel(x, y);
            lum[static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(x)] =
                0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
        }
    }
    auto L = [&](int x, int y) {
        x = x < 0 ? 0 : (x >= w ? w - 1 : x);
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);
        return lum[static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(x)];
    };
    // Sobel gradients.
    std::vector<float> ixx(lum.size(), 0.0f), iyy(lum.size(), 0.0f), ixy(lum.size(), 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float gx = (L(x + 1, y - 1) + 2.0f * L(x + 1, y) + L(x + 1, y + 1)) -
                             (L(x - 1, y - 1) + 2.0f * L(x - 1, y) + L(x - 1, y + 1));
            const float gy = (L(x - 1, y + 1) + 2.0f * L(x, y + 1) + L(x + 1, y + 1)) -
                             (L(x - 1, y - 1) + 2.0f * L(x, y - 1) + L(x + 1, y - 1));
            const std::size_t idx = static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(x);
            ixx[idx] = gx * gx;
            iyy[idx] = gy * gy;
            ixy[idx] = gx * gy;
        }
    }
    // Windowed structure tensor + Harris response.
    std::vector<float> resp(lum.size(), 0.0f);
    const int win = 1;
    float maxR = 0.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sxx = 0.0f, syy = 0.0f, sxy = 0.0f;
            for (int dy = -win; dy <= win; ++dy) {
                for (int dx = -win; dx <= win; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                        continue;
                    }
                    const std::size_t n = static_cast<std::size_t>(ny) * wz + static_cast<std::size_t>(nx);
                    sxx += ixx[n];
                    syy += iyy[n];
                    sxy += ixy[n];
                }
            }
            const float det = sxx * syy - sxy * sxy;
            const float trace = sxx + syy;
            const float r = det - k * trace * trace;
            resp[static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(x)] = r;
            maxR = std::max(maxR, r);
        }
    }
    if (maxR <= 1e-12f) {
        return corners;
    }
    const float thresh = relThreshold * maxR;
    // Non-maximum suppression.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float r = resp[static_cast<std::size_t>(y) * wz + static_cast<std::size_t>(x)];
            if (r < thresh) {
                continue;
            }
            bool isMax = true;
            for (int dy = -nmsRadius; dy <= nmsRadius && isMax; ++dy) {
                for (int dx = -nmsRadius; dx <= nmsRadius; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h || (dx == 0 && dy == 0)) {
                        continue;
                    }
                    if (resp[static_cast<std::size_t>(ny) * wz + static_cast<std::size_t>(nx)] > r) {
                        isMax = false;
                        break;
                    }
                }
            }
            if (isMax) {
                corners.push_back(Corner{x, y, r});
            }
        }
    }
    return corners;
}

} // namespace maz::render
