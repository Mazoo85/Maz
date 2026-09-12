#pragma once

#include "maz/film/Palette.hpp"
#include "maz/render/Image.hpp"
#include "maz/render/Path.hpp"
#include "maz/render/PathFill.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::film::Canvas — the small drawing surface the fifteen sets are written against.
//
// The sets in film/js/film-sets.js are 500 lines of canvas-2D calls, and porting them by hand is only
// sane if the target reads the same way the source does. So this offers the SUBSET those sets actually
// use -- counted, not guessed: 148 fillRects, 123 fill-style changes, 17 paths, 14 gradients, 6 arcs,
// 4 strokes, 3 saves, 2 clips -- and nothing else. It is deliberately not a canvas-2D implementation;
// it is the shape of one, only as far as the sets reach.
//
// Everything is drawn in WORLD space (1000 x 420, the size a set is composed in) and mapped to the
// image through one affine set by the caller, which is how a set gets its parallax plane, its zoom and
// its roll without knowing anything about any of them.
namespace maz::film {

// A colour ramp between two points, or between two radii. Built through the canvas so a set reads the
// way it does in the browser.
struct Gradient {
    bool radial = false;
    float x0 = 0.0f, y0 = 0.0f, r0 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f, r1 = 0.0f;
    std::vector<std::pair<float, render::Color>> stops; // offset 0..1, colour

    void addStop(float offset, const render::Color& c) { stops.emplace_back(offset, c); }

    // The colour at a point, in the gradient's own (world) space.
    render::Color at(float x, float y) const {
        if (stops.empty()) {
            return render::Color{0.0f, 0.0f, 0.0f, 0.0f};
        }
        float t = 0.0f;
        if (radial) {
            const float dx = x - x1;
            const float dy = y - y1;
            const float d = std::sqrt(dx * dx + dy * dy);
            t = r1 > r0 ? (d - r0) / (r1 - r0) : 0.0f;
        } else {
            const float dx = x1 - x0;
            const float dy = y1 - y0;
            const float len2 = dx * dx + dy * dy;
            t = len2 > 0.0f ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0.0f;
        }
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        // Walk the stops. There are never more than a handful, so a scan is the whole story.
        if (t <= stops.front().first) {
            return stops.front().second;
        }
        for (std::size_t i = 0; i + 1 < stops.size(); ++i) {
            const float a = stops[i].first;
            const float b = stops[i + 1].first;
            if (t >= a && t <= b) {
                const float u = b > a ? (t - a) / (b - a) : 0.0f;
                const render::Color& ca = stops[i].second;
                const render::Color& cb = stops[i + 1].second;
                return render::Color{ca.r + (cb.r - ca.r) * u, ca.g + (cb.g - ca.g) * u,
                                     ca.b + (cb.b - ca.b) * u, ca.a + (cb.a - ca.a) * u};
            }
        }
        return stops.back().second;
    }
};

class Canvas {
public:
    Canvas(render::Image& img, int frameTop, int frameHeight)
        : m_img(img), m_clipTop(frameTop), m_clipBottom(frameTop + frameHeight) {}

    // --- the transform -------------------------------------------------------------------------
    //
    // One affine from world space to the image, set per plane. The sets never touch it.
    void setTransform(float a, float b, float c, float d, float e, float f) {
        m_a = a; m_b = b; m_c = c; m_d = d; m_e = e; m_f = f;
    }

    // --- paint ---------------------------------------------------------------------------------

    void setFill(const render::Color& c) {
        m_fill = c;
        m_useGradient = false;
    }
    // Convenience matching the sets' own `rgb(p.key, 0.4)` idiom.
    void setFill(const Rgb& c, float alpha = 1.0f) { setFill(c.toColor(alpha)); }

    void setFillGradient(const Gradient& g) {
        m_gradient = g;
        m_useGradient = true;
    }

    void setStroke(const render::Color& c) { m_stroke = c; }
    void setStroke(const Rgb& c, float alpha = 1.0f) { m_stroke = c.toColor(alpha); }
    void setLineWidth(float w) { m_lineWidth = w; }

    // --- state ---------------------------------------------------------------------------------

    void save() {
        m_stack.push_back(State{m_fill, m_stroke, m_gradient, m_useGradient, m_lineWidth, m_clipTop,
                                m_clipBottom, m_clipLeft, m_clipRight});
    }
    void restore() {
        if (m_stack.empty()) {
            return;
        }
        const State& s = m_stack.back();
        m_fill = s.fill;
        m_stroke = s.stroke;
        m_gradient = s.gradient;
        m_useGradient = s.useGradient;
        m_lineWidth = s.lineWidth;
        m_clipTop = s.clipTop;
        m_clipBottom = s.clipBottom;
        m_clipLeft = s.clipLeft;
        m_clipRight = s.clipRight;
        m_stack.pop_back();
    }

    // Narrow drawing to a world-space rectangle, on top of whatever clip is already in force. Only
    // rectangles, because that is all the sets ever clip to -- and it keeps the clip a pair of integer
    // bounds the shader can test, rather than a second coverage buffer.
    void clipRect(float x, float y, float w, float h) {
        float dx0 = 0.0f, dy0 = 0.0f, dx1 = 0.0f, dy1 = 0.0f;
        toDevice(x, y, dx0, dy0);
        toDevice(x + w, y + h, dx1, dy1);
        if (dx1 < dx0) std::swap(dx0, dx1);
        if (dy1 < dy0) std::swap(dy0, dy1);
        m_clipLeft = std::max(m_clipLeft, static_cast<int>(std::floor(dx0)));
        m_clipRight = std::min(m_clipRight, static_cast<int>(std::ceil(dx1)));
        m_clipTop = std::max(m_clipTop, static_cast<int>(std::floor(dy0)));
        m_clipBottom = std::min(m_clipBottom, static_cast<int>(std::ceil(dy1)));
    }

    // --- shapes --------------------------------------------------------------------------------

    void fillRect(float x, float y, float w, float h) {
        render::Path p;
        p.rect(x, y, w, h);
        fillWorldPath(p);
    }

    void beginPath() { m_path.clear(); }
    void moveTo(float x, float y) { m_path.moveTo(x, y); }
    void lineTo(float x, float y) { m_path.lineTo(x, y); }
    void quadTo(float cx, float cy, float x, float y) { m_path.quadTo(cx, cy, x, y); }
    void closePath() { m_path.close(); }

    // A full or partial circle, as its own sub-path. The sets only ever want whole ones, so the
    // angles the browser's arc() takes are not offered rather than offered and ignored.
    void circle(float cx, float cy, float r) { m_path.ellipse(cx, cy, r, r); }
    void ellipse(float cx, float cy, float rx, float ry, float rotation = 0.0f) {
        m_path.ellipse(cx, cy, rx, ry, rotation);
    }

    void fill(render::FillRule rule = render::FillRule::NonZero) { fillWorldPath(m_path, rule); }

    // Stroke the path under construction at the current line width. Built as quads with round joins,
    // the same way text is, and filled as one shape so overlapping corners do not darken.
    void stroke() {
        render::Path outline;
        const float half = m_lineWidth * 0.5f;
        for (const auto& contour : m_path.contours()) {
            for (const auto& pt : contour) {
                outline.ellipse(pt.x, pt.y, half, half, 0.0f, /*reversed*/ true);
            }
            for (std::size_t i = 0; i + 1 < contour.size(); ++i) {
                const float dx = contour[i + 1].x - contour[i].x;
                const float dy = contour[i + 1].y - contour[i].y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-6f) {
                    continue;
                }
                const float nx = -dy / len * half;
                const float ny = dx / len * half;
                outline.moveTo(contour[i].x + nx, contour[i].y + ny);
                outline.lineTo(contour[i + 1].x + nx, contour[i + 1].y + ny);
                outline.lineTo(contour[i + 1].x - nx, contour[i + 1].y - ny);
                outline.lineTo(contour[i].x - nx, contour[i].y - ny);
                outline.close();
            }
        }
        const render::Color keep = m_fill;
        const bool keepGrad = m_useGradient;
        m_fill = m_stroke;
        m_useGradient = false;
        fillWorldPath(outline);
        m_fill = keep;
        m_useGradient = keepGrad;
    }

    // Stroke a rectangle's outline, which is all the sets ever stroke besides a path.
    void strokeRect(float x, float y, float w, float h) {
        beginPath();
        moveTo(x, y);
        lineTo(x + w, y);
        lineTo(x + w, y + h);
        lineTo(x, y + h);
        lineTo(x, y);
        stroke();
    }

    // A gradient in world space, ready for setFillGradient.
    static Gradient linearGradient(float x0, float y0, float x1, float y1) {
        Gradient g;
        g.radial = false;
        g.x0 = x0; g.y0 = y0; g.x1 = x1; g.y1 = y1;
        return g;
    }
    static Gradient radialGradient(float x0, float y0, float r0, float x1, float y1, float r1) {
        Gradient g;
        g.radial = true;
        g.x0 = x0; g.y0 = y0; g.r0 = r0; g.x1 = x1; g.y1 = y1; g.r1 = r1;
        return g;
    }

private:
    struct State {
        render::Color fill;
        render::Color stroke;
        Gradient gradient;
        bool useGradient;
        float lineWidth;
        int clipTop, clipBottom, clipLeft, clipRight;
    };

    void toDevice(float x, float y, float& ox, float& oy) const {
        ox = m_a * x + m_c * y + m_e;
        oy = m_b * x + m_d * y + m_f;
    }

    // Invert the transform, so a gradient can be evaluated in the world space it was defined in
    // while the rasterizer walks device pixels.
    bool toWorld(float dx, float dy, float& ox, float& oy) const {
        const float det = m_a * m_d - m_b * m_c;
        if (std::fabs(det) < 1e-12f) {
            return false;
        }
        const float px = dx - m_e;
        const float py = dy - m_f;
        ox = (px * m_d - py * m_c) / det;
        oy = (py * m_a - px * m_b) / det;
        return true;
    }

    void fillWorldPath(const render::Path& world,
                       render::FillRule rule = render::FillRule::NonZero) {
        render::Path device = world;
        device.transform(m_a, m_b, m_c, m_d, m_e, m_f);

        const int top = m_clipTop, bottom = m_clipBottom, left = m_clipLeft, right = m_clipRight;
        if (bottom <= top || right <= left) {
            return;
        }
        const render::Color transparent{0.0f, 0.0f, 0.0f, 0.0f};

        if (!m_useGradient) {
            const render::Color flat = m_fill;
            render::fillPathShaded(
                m_img, device,
                [&](int x, int y) -> render::Color {
                    return (y < top || y >= bottom || x < left || x >= right) ? transparent : flat;
                },
                rule);
            return;
        }
        const Gradient grad = m_gradient;
        render::fillPathShaded(
            m_img, device,
            [&](int x, int y) -> render::Color {
                if (y < top || y >= bottom || x < left || x >= right) {
                    return transparent;
                }
                float wx = 0.0f, wy = 0.0f;
                if (!toWorld(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, wx, wy)) {
                    return transparent;
                }
                return grad.at(wx, wy);
            },
            rule);
    }

    render::Image& m_img;
    render::Path m_path;
    std::vector<State> m_stack;

    render::Color m_fill{1.0f, 1.0f, 1.0f, 1.0f};
    render::Color m_stroke{1.0f, 1.0f, 1.0f, 1.0f};
    Gradient m_gradient;
    bool m_useGradient = false;
    float m_lineWidth = 1.0f;

    int m_clipTop = 0;
    int m_clipBottom = 0;
    int m_clipLeft = -1 << 24;
    int m_clipRight = 1 << 24;

    float m_a = 1.0f, m_b = 0.0f, m_c = 0.0f, m_d = 1.0f, m_e = 0.0f, m_f = 0.0f;
};

} // namespace maz::film
