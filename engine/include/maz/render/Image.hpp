#pragma once

#include "maz/render/ColorOps.hpp" // Color, color8, detail::to255

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// maz::render CPU image — a headless RGBA8 raster, the counterpart to Godot's Image. Games use it to
// build procedural textures, icons and lookup tables on the CPU, edit pixels, flip/blit regions, and
// then hand the raw bytes to the GPU (createTexture). Storage is 8-bit-per-channel, row-major, with a
// TOP-LEFT origin (y grows downward, matching Godot Image). Pure value type, header-only, unit-tested.
namespace maz::render {

class Image {
public:
    Image() = default;
    Image(int width, int height, const Color& fillColor = Color{0.0f, 0.0f, 0.0f, 0.0f})
        : m_w(width < 0 ? 0 : width), m_h(height < 0 ? 0 : height),
          m_px(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h) * 4u, 0u) {
        fill(fillColor);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool empty() const { return m_w == 0 || m_h == 0; }
    // Raw RGBA8 bytes, rows top-to-bottom — ready for Renderer::createTexture.
    const std::vector<std::uint8_t>& data() const { return m_px; }

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }

    // Pixel at (x,y) as a Color; out-of-bounds returns transparent black (a safe default rather than UB).
    Color getPixel(int x, int y) const {
        if (!inBounds(x, y)) {
            return Color{0.0f, 0.0f, 0.0f, 0.0f};
        }
        const std::size_t i = idx(x, y);
        return color8(static_cast<int>(m_px[i]), static_cast<int>(m_px[i + 1]),
                      static_cast<int>(m_px[i + 2]), static_cast<int>(m_px[i + 3]));
    }
    // Set the pixel at (x,y) — Godot Image.set_pixel; out-of-bounds is ignored.
    void setPixel(int x, int y, const Color& c) {
        if (!inBounds(x, y)) {
            return;
        }
        const std::size_t i = idx(x, y);
        m_px[i] = to8(c.r);
        m_px[i + 1] = to8(c.g);
        m_px[i + 2] = to8(c.b);
        m_px[i + 3] = to8(c.a);
    }
    // Fill the whole image with one colour — Godot Image.fill.
    void fill(const Color& c) {
        const std::uint8_t r = to8(c.r), g = to8(c.g), b = to8(c.b), a = to8(c.a);
        for (std::size_t i = 0; i + 3 < m_px.size(); i += 4) {
            m_px[i] = r;
            m_px[i + 1] = g;
            m_px[i + 2] = b;
            m_px[i + 3] = a;
        }
    }
    // Mirror horizontally — Godot Image.flip_x.
    void flipX() {
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w / 2; ++x) {
                swapPx(x, y, m_w - 1 - x, y);
            }
        }
    }
    // Mirror vertically — Godot Image.flip_y.
    void flipY() {
        for (int y = 0; y < m_h / 2; ++y) {
            for (int x = 0; x < m_w; ++x) {
                swapPx(x, y, x, m_h - 1 - y);
            }
        }
    }
    // Copy a `w x h` sub-rectangle of `src` starting at (srcX,srcY) into this image at (dstX,dstY) —
    // Godot Image.blit_rect. Clipped to both images; out-of-range pixels are skipped.
    void blitRect(const Image& src, int srcX, int srcY, int w, int h, int dstX, int dstY) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const int sx = srcX + i, sy = srcY + j, dx = dstX + i, dy = dstY + j;
                if (src.inBounds(sx, sy) && inBounds(dx, dy)) {
                    setPixel(dx, dy, src.getPixel(sx, sy));
                }
            }
        }
    }
    // Alpha-composite a `w x h` sub-rectangle of `src` (from srcX,srcY) OVER this image at
    // (dstX,dstY) — Godot Image.blend_rect. Unlike blitRect (which overwrites), each source pixel is
    // source-over blended onto the destination (render::blend), so semi-transparent pixels mix.
    void blendRect(const Image& src, int srcX, int srcY, int w, int h, int dstX, int dstY) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const int sx = srcX + i, sy = srcY + j, dx = dstX + i, dy = dstY + j;
                if (src.inBounds(sx, sy) && inBounds(dx, dy)) {
                    setPixel(dx, dy, blend(getPixel(dx, dy), src.getPixel(sx, sy)));
                }
            }
        }
    }
    // Fill a `w x h` rectangle at (x,y) with one colour — Godot Image.fill_rect. Clipped to bounds.
    void fillRect(int x, int y, int w, int h, const Color& c) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                setPixel(x + i, y + j, c); // setPixel already ignores out-of-bounds
            }
        }
    }
    // Return a new `w x h` image copied from the sub-rectangle at (x,y) — Godot Image.get_region.
    // Pixels outside this image are transparent black (the new image's default fill).
    Image getRegion(int x, int y, int w, int h) const {
        Image out(w, h);
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const int sx = x + i, sy = y + j;
                if (inBounds(sx, sy)) {
                    out.setPixel(i, j, getPixel(sx, sy));
                }
            }
        }
        return out;
    }
    // Resize the canvas to newW x newH IN PLACE, keeping the top-left corner — Godot Image.crop.
    // Shrinking discards the far edges; growing pads the new area with transparent black.
    void crop(int newW, int newH) {
        Image out(newW, newH);
        const int cw = std::min(newW, m_w), ch = std::min(newH, m_h);
        for (int y = 0; y < ch; ++y) {
            for (int x = 0; x < cw; ++x) {
                out.setPixel(x, y, getPixel(x, y));
            }
        }
        *this = std::move(out);
    }
    // Rotate 90 degrees IN PLACE — Godot Image.rotate_90. Width and height swap. `clockwise` true
    // rotates CW (top row becomes the right column); false rotates CCW (top row becomes left column).
    void rotate90(bool clockwise = true) {
        Image out(m_h, m_w);
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                if (clockwise) {
                    out.setPixel(m_h - 1 - y, x, getPixel(x, y));
                } else {
                    out.setPixel(y, m_w - 1 - x, getPixel(x, y));
                }
            }
        }
        *this = std::move(out);
    }
    // Rotate 180 degrees IN PLACE — Godot Image.rotate_180. Dimensions unchanged; every pixel maps
    // to the diagonally opposite position.
    void rotate180() {
        Image out(m_w, m_h);
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                out.setPixel(m_w - 1 - x, m_h - 1 - y, getPixel(x, y));
            }
        }
        *this = std::move(out);
    }
    // Resampling filter for resize() — Godot Image.Interpolation (nearest and bilinear subset).
    enum class Interpolation { Nearest, Bilinear };
    // Resample to newW x newH IN PLACE — Godot Image.resize. Nearest picks the closest source texel;
    // bilinear (the default, matching Godot) samples at the destination pixel centre and blends the
    // four surrounding texels, clamping at the edges. A non-positive size or an empty source yields
    // an empty image.
    void resize(int newW, int newH, Interpolation interp = Interpolation::Bilinear) {
        if (newW <= 0 || newH <= 0 || empty()) {
            *this = Image(newW < 0 ? 0 : newW, newH < 0 ? 0 : newH);
            return;
        }
        Image out(newW, newH);
        if (interp == Interpolation::Nearest) {
            for (int y = 0; y < newH; ++y) {
                for (int x = 0; x < newW; ++x) {
                    int sx = static_cast<int>(static_cast<long long>(x) * m_w / newW);
                    int sy = static_cast<int>(static_cast<long long>(y) * m_h / newH);
                    sx = std::min(sx, m_w - 1);
                    sy = std::min(sy, m_h - 1);
                    out.setPixel(x, y, getPixel(sx, sy));
                }
            }
        } else {
            const float rx = static_cast<float>(m_w) / static_cast<float>(newW);
            const float ry = static_cast<float>(m_h) / static_cast<float>(newH);
            for (int y = 0; y < newH; ++y) {
                for (int x = 0; x < newW; ++x) {
                    const float fx = (static_cast<float>(x) + 0.5f) * rx - 0.5f;
                    const float fy = (static_cast<float>(y) + 0.5f) * ry - 0.5f;
                    const int x0 = static_cast<int>(std::floor(fx));
                    const int y0 = static_cast<int>(std::floor(fy));
                    const float tx = fx - static_cast<float>(x0);
                    const float ty = fy - static_cast<float>(y0);
                    const int cx0 = std::clamp(x0, 0, m_w - 1);
                    const int cx1 = std::clamp(x0 + 1, 0, m_w - 1);
                    const int cy0 = std::clamp(y0, 0, m_h - 1);
                    const int cy1 = std::clamp(y0 + 1, 0, m_h - 1);
                    const Color top = lerpColor(getPixel(cx0, cy0), getPixel(cx1, cy0), tx);
                    const Color bot = lerpColor(getPixel(cx0, cy1), getPixel(cx1, cy1), tx);
                    out.setPixel(x, y, lerpColor(top, bot, ty));
                }
            }
        }
        *this = std::move(out);
    }

private:
    static Color lerpColor(const Color& a, const Color& b, float t) {
        return Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                     a.a + (b.a - a.a) * t};
    }
    static std::uint8_t to8(float v) { return static_cast<std::uint8_t>(detail::to255(v)); }
    std::size_t idx(int x, int y) const {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
                static_cast<std::size_t>(x)) *
               4u;
    }
    void swapPx(int x0, int y0, int x1, int y1) {
        const std::size_t a = idx(x0, y0), b = idx(x1, y1);
        for (int k = 0; k < 4; ++k) {
            std::swap(m_px[a + static_cast<std::size_t>(k)], m_px[b + static_cast<std::size_t>(k)]);
        }
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<std::uint8_t> m_px;
};

} // namespace maz::render
