// A tiny CPU RGB framebuffer — the software (reference) rasterizer's target.
// Dependency-free so the render path is testable headless; the same drawing
// intent maps onto the Vulkan sprite batch later.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "zomboid/Math.hpp"

namespace zb {

struct Color {
    uint8_t r = 0, g = 0, b = 0;
};

// Build a Color from a 0xRRGGBB hex literal (matches the JS neon palette).
constexpr Color rgb(uint32_t hex) {
    return Color{static_cast<uint8_t>((hex >> 16) & 0xFF),
                 static_cast<uint8_t>((hex >> 8) & 0xFF),
                 static_cast<uint8_t>(hex & 0xFF)};
}

class Framebuffer {
public:
    Framebuffer(int w, int h)
        : m_w(w), m_h(h),
          m_px(static_cast<size_t>(w) * static_cast<size_t>(h) * 3, 0) {}

    int width() const { return m_w; }
    int height() const { return m_h; }
    const std::vector<uint8_t>& pixels() const { return m_px; }

    void clear(Color c) {
        for (size_t i = 0; i < m_px.size(); i += 3) {
            m_px[i] = c.r;
            m_px[i + 1] = c.g;
            m_px[i + 2] = c.b;
        }
    }

    void setPixel(int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) return;
        const size_t i = idx(x, y);
        m_px[i] = c.r;
        m_px[i + 1] = c.g;
        m_px[i + 2] = c.b;
    }

    // Alpha-composite c over the existing pixel (a in [0,1]).
    void blendPixel(int x, int y, Color c, float a) {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) return;
        if (a <= 0.0f) return;
        if (a > 1.0f) a = 1.0f;
        const size_t i = idx(x, y);
        const float ia = 1.0f - a;
        m_px[i] = mix(m_px[i], c.r, a, ia);
        m_px[i + 1] = mix(m_px[i + 1], c.g, a, ia);
        m_px[i + 2] = mix(m_px[i + 2], c.b, a, ia);
    }

    void fillRect(int x, int y, int w, int h, Color c) {
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) setPixel(xx, yy, c);
    }

    void blendRect(int x, int y, int w, int h, Color c, float a) {
        for (int yy = y; yy < y + h; yy++)
            for (int xx = x; xx < x + w; xx++) blendPixel(xx, yy, c, a);
    }

    // Full-frame flat overlay (used for the night darkness pass).
    void overlay(Color c, float a) { blendRect(0, 0, m_w, m_h, c, a); }

    void outlineRect(int x, int y, int w, int h, Color c) {
        for (int xx = x; xx < x + w; xx++) {
            setPixel(xx, y, c);
            setPixel(xx, y + h - 1, c);
        }
        for (int yy = y; yy < y + h; yy++) {
            setPixel(x, yy, c);
            setPixel(x + w - 1, yy, c);
        }
    }

    void fillCircle(int cx, int cy, int r, Color c) {
        for (int yy = -r; yy <= r; yy++)
            for (int xx = -r; xx <= r; xx++)
                if (xx * xx + yy * yy <= r * r) setPixel(cx + xx, cy + yy, c);
    }

    // Thick line via a simple DDA with a square brush.
    void drawLine(int x0, int y0, int x1, int y1, Color c, int thick = 1) {
        const int dx = x1 - x0, dy = y1 - y0;
        const int steps = std::max(std::abs(dx), std::abs(dy));
        if (steps == 0) {
            stamp(x0, y0, thick, c);
            return;
        }
        for (int i = 0; i <= steps; i++) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int x = x0 + static_cast<int>(std::lround(static_cast<float>(dx) * t));
            const int y = y0 + static_cast<int>(std::lround(static_cast<float>(dy) * t));
            stamp(x, y, thick, c);
        }
    }

private:
    // Byte offset of pixel (x,y); callers guard x,y >= 0 before use.
    size_t idx(int x, int y) const {
        return (static_cast<size_t>(y) * static_cast<size_t>(m_w) + static_cast<size_t>(x)) * 3;
    }
    static uint8_t mix(uint8_t dst, uint8_t src, float a, float ia) {
        return static_cast<uint8_t>(static_cast<float>(src) * a + static_cast<float>(dst) * ia);
    }
    void stamp(int x, int y, int thick, Color c) {
        if (thick <= 1) {
            setPixel(x, y, c);
            return;
        }
        const int h = thick / 2;
        fillRect(x - h, y - h, thick, thick, c);
    }

    int m_w, m_h;
    std::vector<uint8_t> m_px; // RGB8, row-major
};

} // namespace zb
