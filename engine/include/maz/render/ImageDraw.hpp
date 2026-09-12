#pragma once

#include "maz/render/Image.hpp" // render::Image, render::Color

#include <algorithm>

// maz::render image drawing primitives — rasterize 2D shapes directly INTO an Image on the CPU. The Image
// class already edits pixels and blits regions, but it had no way to stroke a line, outline or fill a circle,
// draw a rectangle border, or fill a triangle — the building blocks for procedural textures, generated icons,
// minimap/radar overlays, debug visualisations, and simple CPU-side vector art. These free functions plot
// through the Image's bounds-checked setPixel, so anything off-canvas is safely clipped. Integer pixel
// coordinates; the classic Bresenham line and midpoint circle, a bounding-box disc fill, and a barycentric
// triangle fill (winding-independent). Header-only, pure, deterministic. (Distinct from Renderer's GPU
// debug-draw — this writes into an in-memory image you can then save, upload, or sample.)
namespace maz::render {

// Bresenham line from (x0,y0) to (x1,y1) inclusive.
inline void drawLine(Image& img, int x0, int y0, int x1, int y1, const Color& c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (img.inBounds(x0, y0)) img.setPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Rectangle OUTLINE with top-left (x,y) and size w x h (use Image::fillRect for a solid rectangle).
inline void drawRect(Image& img, int x, int y, int w, int h, const Color& c) {
    if (w <= 0 || h <= 0) return;
    drawLine(img, x, y, x + w - 1, y, c);                 // top
    drawLine(img, x, y + h - 1, x + w - 1, y + h - 1, c);  // bottom
    drawLine(img, x, y, x, y + h - 1, c);                  // left
    drawLine(img, x + w - 1, y, x + w - 1, y + h - 1, c);  // right
}

// Circle OUTLINE of radius r centred at (cx,cy) — midpoint circle algorithm (8-way symmetry).
inline void drawCircle(Image& img, int cx, int cy, int r, const Color& c) {
    if (r < 0) return;
    auto plot = [&](int px, int py) { if (img.inBounds(px, py)) img.setPixel(px, py, c); };
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        plot(cx + x, cy + y); plot(cx - x, cy + y);
        plot(cx + x, cy - y); plot(cx - x, cy - y);
        plot(cx + y, cy + x); plot(cx - y, cy + x);
        plot(cx + y, cy - x); plot(cx - y, cy - x);
        ++y;
        if (d < 0) { d += 2 * y + 1; }
        else { --x; d += 2 * (y - x) + 1; }
    }
}

// Solid disc of radius r centred at (cx,cy).
inline void fillCircle(Image& img, int cx, int cy, int r, const Color& c) {
    if (r < 0) return;
    const int r2 = r * r;
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r2 && img.inBounds(cx + x, cy + y)) img.setPixel(cx + x, cy + y, c);
}

namespace detail {
inline int edge(int ax, int ay, int bx, int by, int px, int py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
} // namespace detail

// Solid triangle (winding-independent) via a barycentric bounding-box scan.
inline void fillTriangle(Image& img, int x0, int y0, int x1, int y1, int x2, int y2, const Color& c) {
    int minX = std::min(x0, std::min(x1, x2));
    int maxX = std::max(x0, std::max(x1, x2));
    int minY = std::min(y0, std::min(y1, y2));
    int maxY = std::max(y0, std::max(y1, y2));
    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, img.width() - 1);
    maxY = std::min(maxY, img.height() - 1);
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const int w0 = detail::edge(x1, y1, x2, y2, px, py);
            const int w1 = detail::edge(x2, y2, x0, y0, px, py);
            const int w2 = detail::edge(x0, y0, x1, y1, px, py);
            const bool allNonNeg = w0 >= 0 && w1 >= 0 && w2 >= 0;
            const bool allNonPos = w0 <= 0 && w1 <= 0 && w2 <= 0;
            if (allNonNeg || allNonPos) img.setPixel(px, py, c); // inside for either winding
        }
    }
}

} // namespace maz::render
