#pragma once

#include "maz/render/Image.hpp" // Image, Color

#include <cmath>

// maz::render::drawLineAA — Xiaolin Wu's anti-aliased line. The engine's Bresenham drawLine (ImageDraw.hpp)
// snaps each step to one pixel, so any non-axis-aligned line comes out jagged ("staircased"). Wu's algorithm
// instead spreads each step across the TWO pixels it straddles, weighted by how much of the pixel the line
// actually covers, producing smooth edges — exactly what crisp graph plots, wireframe overlays, vector-style
// UI strokes, minimap routes, and debug gizmos want on a CPU raster. It also takes sub-pixel float endpoints,
// so lines can start and end between pixels. Coverage is alpha-composited over whatever is already in the
// image. Godot's Image has no anti-aliased line primitive. Header-only, std-only, deterministic.
namespace maz::render {

namespace detail {

// Alpha-composite `c` over the pixel at (x,y) with coverage `cov` in [0,1].
inline void blendCoverage(Image& img, int x, int y, const Color& c, float cov) {
    if (cov <= 0.0f || !img.inBounds(x, y)) {
        return;
    }
    if (cov > 1.0f) {
        cov = 1.0f;
    }
    // Delegates to Image's integer compositor: same result, without a float round trip per channel.
    img.blendPixel(x, y, c, cov);
}

inline float ipartf(float x) { return std::floor(x); }
inline float fpartf(float x) { return x - std::floor(x); }
inline float rfpartf(float x) { return 1.0f - fpartf(x); }
inline int roundi(float x) { return static_cast<int>(std::floor(x + 0.5f)); }

} // namespace detail

// Draw an anti-aliased line from (x0,y0) to (x1,y1) (float, sub-pixel) in colour `c`.
inline void drawLineAA(Image& img, float x0, float y0, float x1, float y1, const Color& c) {
    using namespace detail;
    const bool steep = std::fabs(y1 - y0) > std::fabs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    auto plot = [&](int px, int py, float cov) {
        if (steep) {
            blendCoverage(img, py, px, c, cov);
        } else {
            blendCoverage(img, px, py, c, cov);
        }
    };

    // First endpoint.
    float xend = static_cast<float>(roundi(x0));
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpartf(x0 + 0.5f);
    const int xpxl1 = static_cast<int>(xend);
    const int ypxl1 = static_cast<int>(ipartf(yend));
    plot(xpxl1, ypxl1, rfpartf(yend) * xgap);
    plot(xpxl1, ypxl1 + 1, fpartf(yend) * xgap);
    float intery = yend + gradient;

    // Second endpoint.
    xend = static_cast<float>(roundi(x1));
    yend = y1 + gradient * (xend - x1);
    xgap = fpartf(x1 + 0.5f);
    const int xpxl2 = static_cast<int>(xend);
    const int ypxl2 = static_cast<int>(ipartf(yend));
    plot(xpxl2, ypxl2, rfpartf(yend) * xgap);
    plot(xpxl2, ypxl2 + 1, fpartf(yend) * xgap);

    // Main span.
    for (int x = xpxl1 + 1; x < xpxl2; ++x) {
        const int iy = static_cast<int>(ipartf(intery));
        plot(x, iy, rfpartf(intery));
        plot(x, iy + 1, fpartf(intery));
        intery += gradient;
    }
}

} // namespace maz::render
