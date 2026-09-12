#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render dithering — reduce a grayscale image to a few brightness levels while HIDING the banding
// that naive quantization produces, by trading spatial noise for tonal accuracy. Two classic methods:
// ordered (Bayer-matrix) dithering, which adds a fixed, tileable threshold pattern per pixel (the
// crisp, deterministic look of old console/print art), and Floyd-Steinberg error diffusion, which
// pushes each pixel's rounding error into its not-yet-processed neighbours (smoother, less patterned).
// The companion to color quantization (M444) for retro/1-bit/limited-palette looks, e-ink-style output,
// and stylized post effects — none of which Godot provides. bayerMatrix() also stands alone as a
// reusable threshold-matrix generator. Header-only, std-only, deterministic.
namespace maz::render {

// Bayer ordered-dither threshold matrix, (1<<level) on a side, values 0 .. n*n-1 (row-major).
inline std::vector<int> bayerMatrix(int level) {
    if (level < 0) {
        level = 0;
    }
    const std::size_t n = static_cast<std::size_t>(1) << level;
    std::vector<int> m(1, 0); // 1x1 seed
    std::size_t size = 1;
    while (size < n) {
        const std::size_t ns = size * 2;
        std::vector<int> nm(ns * ns, 0);
        for (std::size_t y = 0; y < size; ++y) {
            for (std::size_t x = 0; x < size; ++x) {
                const int v = m[y * size + x];
                nm[y * ns + x] = 4 * v + 0;
                nm[y * ns + (x + size)] = 4 * v + 2;
                nm[(y + size) * ns + x] = 4 * v + 3;
                nm[(y + size) * ns + (x + size)] = 4 * v + 1;
            }
        }
        m = std::move(nm);
        size = ns;
    }
    return m;
}

// Quantize a grayscale image (row-major, w*h bytes) to `levels` evenly-spaced brightness levels,
// applying an ordered Bayer dither of side 1<<bayerLevel. Output values are the level representatives
// (0, 255/(levels-1), ...). levels is clamped to >= 2.
inline std::vector<std::uint8_t> orderedDitherGray(const std::vector<std::uint8_t>& px, int w, int h,
                                                   int levels, int bayerLevel = 2) {
    if (levels < 2) {
        levels = 2;
    }
    const std::vector<int> bm = bayerMatrix(bayerLevel);
    const std::size_t n = static_cast<std::size_t>(1) << (bayerLevel < 0 ? 0 : bayerLevel);
    const double nn = static_cast<double>(n * n);
    const double step = 255.0 / static_cast<double>(levels - 1);
    const std::size_t ww = static_cast<std::size_t>(w < 0 ? 0 : w);
    std::vector<std::uint8_t> out(px.size(), 0);
    for (std::size_t y = 0; y < static_cast<std::size_t>(h < 0 ? 0 : h); ++y) {
        for (std::size_t x = 0; x < ww; ++x) {
            const std::size_t i = y * ww + x;
            // Threshold offset in (-0.5, 0.5) of one quantization step.
            const double t = (static_cast<double>(bm[(y % n) * n + (x % n)]) + 0.5) / nn - 0.5;
            const double adjusted = static_cast<double>(px[i]) + t * step;
            int q = static_cast<int>(std::lround(adjusted / step));
            q = q < 0 ? 0 : (q > levels - 1 ? levels - 1 : q);
            out[i] = static_cast<std::uint8_t>(std::lround(static_cast<double>(q) * step));
        }
    }
    return out;
}

// Quantize a grayscale image to `levels` levels via Floyd-Steinberg error diffusion. Brightness is
// preserved well because each pixel's rounding error is spread to neighbours (7/16 right, 3/16 down-
// left, 5/16 down, 1/16 down-right). levels is clamped to >= 2.
inline std::vector<std::uint8_t> floydSteinbergGray(const std::vector<std::uint8_t>& px, int w, int h,
                                                    int levels) {
    if (levels < 2) {
        levels = 2;
    }
    const double step = 255.0 / static_cast<double>(levels - 1);
    const int wc = w < 0 ? 0 : w;
    const int hc = h < 0 ? 0 : h;
    std::vector<double> buf(px.begin(), px.end());
    std::vector<std::uint8_t> out(px.size(), 0);
    auto add = [&](int xx, int yy, double e) {
        if (xx >= 0 && xx < wc && yy >= 0 && yy < hc) {
            buf[static_cast<std::size_t>(yy) * static_cast<std::size_t>(wc) +
                static_cast<std::size_t>(xx)] += e;
        }
    };
    for (int y = 0; y < hc; ++y) {
        for (int x = 0; x < wc; ++x) {
            const std::size_t i =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(wc) + static_cast<std::size_t>(x);
            const double oldv = buf[i];
            int q = static_cast<int>(std::lround(oldv / step));
            q = q < 0 ? 0 : (q > levels - 1 ? levels - 1 : q);
            const double newv = static_cast<double>(q) * step;
            out[i] = static_cast<std::uint8_t>(newv);
            const double err = oldv - newv;
            add(x + 1, y, err * (7.0 / 16.0));
            add(x - 1, y + 1, err * (3.0 / 16.0));
            add(x, y + 1, err * (5.0 / 16.0));
            add(x + 1, y + 1, err * (1.0 / 16.0));
        }
    }
    return out;
}

} // namespace maz::render
