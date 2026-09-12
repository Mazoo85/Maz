#pragma once

#include <cstddef>
#include <vector>

// maz::math Haar wavelet transform — the simplest multi-resolution transform: it repeatedly splits a signal
// (or image) into a coarse "average" half and a fine "detail" half, so a texture or heightfield becomes a
// small blurry thumbnail plus a stack of ever-finer correction layers. That decomposition is the backbone
// of progressive/streamed loading (show the thumbnail, refine as detail arrives), level-of-detail, and
// lossy compression (most detail coefficients are tiny — zero the small ones and the picture barely
// changes). This is the normalised (orthonormal) Haar basis, so it PRESERVES ENERGY exactly and inverts
// perfectly. Works on power-of-two 1D arrays and square power-of-two 2D grids, decomposing to the deepest
// level. Godot ships no wavelet transform. Header-only, std-only, deterministic.
namespace maz::math {

namespace detail {
constexpr float kInvSqrt2 = 0.70710678118654752440f;

inline bool isPow2(std::size_t n) { return n >= 1 && (n & (n - 1)) == 0; }

// One in-place Haar step over `size` samples starting at `start`, spaced by `stride`.
inline void haarStepFwd(std::vector<float>& g, std::size_t start, std::size_t stride, std::size_t size) {
    const std::size_t half = size / 2;
    std::vector<float> tmp(size);
    for (std::size_t i = 0; i < half; ++i) {
        const float x0 = g[start + (2 * i) * stride];
        const float x1 = g[start + (2 * i + 1) * stride];
        tmp[i] = (x0 + x1) * kInvSqrt2;        // average (approximation)
        tmp[half + i] = (x0 - x1) * kInvSqrt2; // difference (detail)
    }
    for (std::size_t i = 0; i < size; ++i) {
        g[start + i * stride] = tmp[i];
    }
}

inline void haarStepInv(std::vector<float>& g, std::size_t start, std::size_t stride, std::size_t size) {
    const std::size_t half = size / 2;
    std::vector<float> tmp(size);
    for (std::size_t i = 0; i < half; ++i) {
        const float a = g[start + i * stride];
        const float d = g[start + (half + i) * stride];
        tmp[2 * i] = (a + d) * kInvSqrt2;
        tmp[2 * i + 1] = (a - d) * kInvSqrt2;
    }
    for (std::size_t i = 0; i < size; ++i) {
        g[start + i * stride] = tmp[i];
    }
}
} // namespace detail

// Full multi-level 1D Haar forward transform. Returns the coefficient array (same length as input); input
// length must be a power of two, else the input is returned unchanged.
inline std::vector<float> haarForward1D(const std::vector<float>& signal) {
    std::vector<float> g = signal;
    if (!detail::isPow2(g.size())) {
        return g;
    }
    for (std::size_t size = g.size(); size > 1; size /= 2) {
        detail::haarStepFwd(g, 0, 1, size);
    }
    return g;
}

// Inverse of haarForward1D.
inline std::vector<float> haarInverse1D(const std::vector<float>& coeff) {
    std::vector<float> g = coeff;
    if (!detail::isPow2(g.size())) {
        return g;
    }
    for (std::size_t size = 2; size <= g.size(); size *= 2) {
        detail::haarStepInv(g, 0, 1, size);
    }
    return g;
}

// Full multi-level 2D Haar forward transform of an n x n grid (row-major). `n` must be a power of two and
// grid.size() == n*n, else the grid is returned unchanged. At each scale rows are transformed, then columns,
// then the transform recurses into the top-left LL quadrant.
inline std::vector<float> haarForward2D(const std::vector<float>& grid, std::size_t n) {
    std::vector<float> g = grid;
    if (!detail::isPow2(n) || g.size() != n * n) {
        return g;
    }
    for (std::size_t size = n; size > 1; size /= 2) {
        for (std::size_t y = 0; y < size; ++y) {
            detail::haarStepFwd(g, y * n, 1, size); // row y, first `size` columns
        }
        for (std::size_t x = 0; x < size; ++x) {
            detail::haarStepFwd(g, x, n, size); // column x, first `size` rows
        }
    }
    return g;
}

// Inverse of haarForward2D.
inline std::vector<float> haarInverse2D(const std::vector<float>& coeff, std::size_t n) {
    std::vector<float> g = coeff;
    if (!detail::isPow2(n) || g.size() != n * n) {
        return g;
    }
    for (std::size_t size = 2; size <= n; size *= 2) {
        for (std::size_t x = 0; x < size; ++x) {
            detail::haarStepInv(g, x, n, size); // columns first (reverse of forward order)
        }
        for (std::size_t y = 0; y < size; ++y) {
            detail::haarStepInv(g, y * n, 1, size); // then rows
        }
    }
    return g;
}

} // namespace maz::math
