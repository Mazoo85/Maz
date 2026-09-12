#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render Sobel edge detection — find the edges (sharp brightness changes) in a grayscale image.
//
// The Sobel operator convolves the image with two small 3x3 kernels that estimate the horizontal (Gx)
// and vertical (Gy) brightness gradient at each pixel; the gradient MAGNITUDE sqrt(Gx^2+Gy^2) is large
// exactly where the image changes fast — i.e. on an edge — and the DIRECTION atan2(Gy,Gx) points across
// it. This is the classic building block behind toon/outline post-processing (run it on depth or normals
// to draw ink lines), sprite/UI outline generation, and general image analysis. Borders use clamp-to-edge
// so every pixel gets a value. Pure CPU, header-only, deterministic — unit-tested against hand-computed
// Sobel responses (a flat image gives zero; a vertical step edge gives Gx=+/-4, Gy=0, and vice versa).
namespace maz::render {

struct EdgeField {
    int width = 0;
    int height = 0;
    std::vector<float> gx;  // horizontal gradient (row-major w*h)
    std::vector<float> gy;  // vertical gradient
    std::vector<float> mag; // gradient magnitude sqrt(gx^2 + gy^2)
};

// Sobel gradients of a grayscale image (`gray` is row-major w*h). Clamp-to-edge borders.
inline EdgeField sobel(const float* gray, int w, int h) {
    EdgeField out;
    out.width = w;
    out.height = h;
    if (gray == nullptr || w <= 0 || h <= 0) {
        return out;
    }
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    out.gx.assign(n, 0.0f);
    out.gy.assign(n, 0.0f);
    out.mag.assign(n, 0.0f);

    auto at = [&](int x, int y) -> float {
        if (x < 0) x = 0;
        if (x >= w) x = w - 1;
        if (y < 0) y = 0;
        if (y >= h) y = h - 1;
        return gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float tl = at(x - 1, y - 1), tc = at(x, y - 1), tr = at(x + 1, y - 1);
            const float ml = at(x - 1, y), mr = at(x + 1, y);
            const float bl = at(x - 1, y + 1), bc = at(x, y + 1), br = at(x + 1, y + 1);
            const float gx = (tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl);
            const float gy = (bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr);
            const std::size_t i =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            out.gx[i] = gx;
            out.gy[i] = gy;
            out.mag[i] = std::sqrt(gx * gx + gy * gy);
        }
    }
    return out;
}

inline EdgeField sobel(const std::vector<float>& gray, int w, int h) {
    return sobel(gray.data(), w, h);
}

// Binary edge map: 1 where the gradient magnitude exceeds `threshold`, else 0.
inline std::vector<unsigned char> edgeMask(const EdgeField& e, float threshold) {
    std::vector<unsigned char> mask(e.mag.size(), 0);
    for (std::size_t i = 0; i < e.mag.size(); ++i) {
        mask[i] = e.mag[i] > threshold ? 1u : 0u;
    }
    return mask;
}

} // namespace maz::render
