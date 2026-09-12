#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render CPU image blur — separable Gaussian and box blur over a row-major grayscale float image.
// This is the offline/CPU counterpart to the engine's GPU blur passes (bloom, SSAO): the tool for
// softening procedurally-generated textures and heightmaps, anti-aliasing signed-distance fields,
// baking soft ambient occlusion or shadow into a texture, and building properly-filtered mip levels —
// all headlessly, with no GPU. Godot exposes image blur only on the GPU (via shaders/compositor), so a
// deterministic CPU blur is a genuinely-useful beyond-Godot utility. Boundaries use clamp-to-edge, so a
// constant image is returned unchanged (partition of unity). Header-only, std-only.
namespace maz::render {

// Normalized 1D Gaussian kernel of half-width `radius` (length 2*radius+1). Weights are symmetric and
// sum to exactly 1. A non-positive `sigma` derives sigma = radius/3 (the usual "3-sigma covers the
// kernel" rule). radius <= 0 yields the trivial {1} kernel.
inline std::vector<float> gaussianKernel1D(int radius, float sigma = 0.0f) {
    if (radius < 0) {
        radius = 0;
    }
    if (sigma <= 0.0f) {
        sigma = radius > 0 ? static_cast<float>(radius) / 3.0f : 1.0f;
    }
    const int size = 2 * radius + 1;
    std::vector<float> k(static_cast<std::size_t>(size));
    const float inv2s2 = 1.0f / (2.0f * sigma * sigma);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        const float w = std::exp(-static_cast<float>(i * i) * inv2s2);
        k[static_cast<std::size_t>(i + radius)] = w;
        sum += w;
    }
    const float inv = 1.0f / sum;
    for (float& w : k) {
        w *= inv;
    }
    return k;
}

namespace detail {

// Apply a symmetric 1D `kernel` (half-width `radius`) as two separable passes over a `width` x `height`
// row-major image, clamping sample indices to the edge. Returns a new image.
inline std::vector<float> separableBlur(const std::vector<float>& src, int width, int height, int radius,
                                        const std::vector<float>& kernel) {
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    std::vector<float> tmp(w * h, 0.0f);
    std::vector<float> out(w * h, 0.0f);
    const long wl = static_cast<long>(w);
    const long hl = static_cast<long>(h);

    // Horizontal pass: src -> tmp.
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            float acc = 0.0f;
            for (int t = -radius; t <= radius; ++t) {
                long sx = static_cast<long>(x) + t;
                sx = sx < 0 ? 0 : (sx >= wl ? wl - 1 : sx);
                acc += kernel[static_cast<std::size_t>(t + radius)]
                       * src[y * w + static_cast<std::size_t>(sx)];
            }
            tmp[y * w + x] = acc;
        }
    }
    // Vertical pass: tmp -> out.
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            float acc = 0.0f;
            for (int t = -radius; t <= radius; ++t) {
                long sy = static_cast<long>(y) + t;
                sy = sy < 0 ? 0 : (sy >= hl ? hl - 1 : sy);
                acc += kernel[static_cast<std::size_t>(t + radius)]
                       * tmp[static_cast<std::size_t>(sy) * w + x];
            }
            out[y * w + x] = acc;
        }
    }
    return out;
}

} // namespace detail

// Separable Gaussian blur of a row-major grayscale float image (clamp-to-edge). Returns the source
// unchanged if the dimensions are invalid, the size mismatches, or radius <= 0.
inline std::vector<float> gaussianBlurGray(const std::vector<float>& src, int width, int height,
                                           int radius, float sigma = 0.0f) {
    if (width <= 0 || height <= 0 || radius <= 0
        || src.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return src;
    }
    return detail::separableBlur(src, width, height, radius, gaussianKernel1D(radius, sigma));
}

// Separable box blur (uniform (2r+1)-tap moving average per axis, clamp-to-edge). Cheaper than Gaussian;
// three successive box passes approximate a Gaussian. Same fall-through rules as gaussianBlurGray.
inline std::vector<float> boxBlurGray(const std::vector<float>& src, int width, int height, int radius) {
    if (width <= 0 || height <= 0 || radius <= 0
        || src.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return src;
    }
    const std::size_t size = static_cast<std::size_t>(2 * radius + 1);
    const std::vector<float> kernel(size, 1.0f / static_cast<float>(size));
    return detail::separableBlur(src, width, height, radius, kernel);
}

} // namespace maz::render
