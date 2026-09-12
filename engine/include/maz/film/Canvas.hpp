#pragma once

#include "maz/film/Palette.hpp"
#include "maz/render/Image.hpp"
#include "maz/render/Path.hpp"
#include "maz/render/LineAA.hpp" // detail::blendCoverage
#include "maz/render/PathFill.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
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

    // The baked ramp. Mutable because baking is a cache of what the stops already say, not a change
    // to the gradient -- `at()` on a const gradient must still be able to fill it.
    mutable std::array<render::Color, 256> ramp{};
    mutable bool baked = false;

    void addStop(float offset, const render::Color& c) {
        stops.emplace_back(offset, c);
        baked = false;
    }

    // --- the fast path -----------------------------------------------------------------------------
    //
    // A gradient is evaluated once per PIXEL, and a set's sky or lamp pool is a gradient across most
    // of the frame -- so the corridor, whose back plane is a radial gradient over some 800,000
    // pixels, cost 133ms a frame while the lighthouse cost 21. Three things were being redone per
    // pixel that need not be: walking the stop list, interpolating between two stops, and inverting
    // the transform. Baking the ramp to a small table removes the first two.
    static constexpr int kRampSize = 256;

    void bake() const {
        if (baked) {
            return;
        }
        for (int i = 0; i < kRampSize; ++i) {
            ramp[static_cast<std::size_t>(i)] =
                sample(static_cast<float>(i) / static_cast<float>(kRampSize - 1));
        }
        baked = true;
    }

    // The colour at a parameter already reduced to 0..1 along the ramp.
    const render::Color& atT(float t) const {
        int i = static_cast<int>(t * static_cast<float>(kRampSize - 1) + 0.5f);
        if (i < 0) {
            i = 0;
        }
        if (i >= kRampSize) {
            i = kRampSize - 1;
        }
        return ramp[static_cast<std::size_t>(i)];
    }

    // How far along the ramp a world point sits, before clamping.
    float rawT(float x, float y) const {
        if (radial) {
            const float dx = x - x1;
            const float dy = y - y1;
            const float d = std::sqrt(dx * dx + dy * dy);
            return r1 > r0 ? (d - r0) / (r1 - r0) : 0.0f;
        }
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float len2 = dx * dx + dy * dy;
        return len2 > 0.0f ? ((x - x0) * dx + (y - y0) * dy) / len2 : 0.0f;
    }

    // The colour at a point, in the gradient's own (world) space.
    render::Color at(float x, float y) const { return sample(rawT(x, y)); }

    render::Color sample(float t) const {
        if (stops.empty()) {
            return render::Color{0.0f, 0.0f, 0.0f, 0.0f};
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
        // Invert once, here. A gradient has to be evaluated in the world space it was defined in
        // while the rasterizer walks device pixels, and solving this inside the shader meant a
        // determinant and two divisions for every pixel of every gradient -- on a set whose sky is a
        // full-frame gradient, that is a million of them per frame.
        const float det = m_a * m_d - m_b * m_c;
        m_invertible = std::fabs(det) > 1e-12f;
        if (m_invertible) {
            m_ia = m_d / det;
            m_ib = -m_b / det;
            m_ic = -m_c / det;
            m_id = m_a / det;
        }
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
        m_gradient.bake();
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

    // A rectangle. Overwhelmingly the most common thing a set draws -- 148 of the roughly 180
    // drawing operations across the fifteen sets -- so it gets an exact fast path instead of going
    // through the general rasterizer.
    //
    // Why it matters: the general path samples each pixel row at 16 sub-scanlines, which is what
    // makes a curve or a diagonal smooth and is entirely wasted on a rectangle, where every
    // sub-scanline of a row has identical coverage. A set whose back plane is a few full-frame
    // rectangles was spending 50ms a frame on that waste. Here the coverage of each pixel is just
    // the overlap of the pixel with the rectangle, computed once.
    //
    // Only when the transform has no rotation in it: under a roll a rectangle is a diamond, and
    // that goes the general way.
    void fillRect(float x, float y, float w, float h) {
        if (m_b != 0.0f || m_c != 0.0f) {
            render::Path p;
            p.rect(x, y, w, h);
            fillWorldPath(p);
            return;
        }
        float dx0 = 0.0f, dy0 = 0.0f, dx1 = 0.0f, dy1 = 0.0f;
        toDevice(x, y, dx0, dy0);
        toDevice(x + w, y + h, dx1, dy1);
        if (dx1 < dx0) std::swap(dx0, dx1);
        if (dy1 < dy0) std::swap(dy0, dy1);

        int left = std::max(m_clipLeft, 0);
        int right = std::min(m_clipRight, m_img.width());
        int top = std::max(m_clipTop, 0);
        int bottom = std::min(m_clipBottom, m_img.height());
        left = std::max(left, static_cast<int>(std::floor(dx0)));
        right = std::min(right, static_cast<int>(std::ceil(dx1)));
        top = std::max(top, static_cast<int>(std::floor(dy0)));
        bottom = std::min(bottom, static_cast<int>(std::ceil(dy1)));
        if (right <= left || bottom <= top) {
            return;
        }

        const Gradient grad = m_gradient;
        const bool useGrad = m_useGradient;
        const render::Color flat = m_fill;

        for (int py = top; py < bottom; ++py) {
            const float rowLo = std::fmax(dy0, static_cast<float>(py));
            const float rowHi = std::fmin(dy1, static_cast<float>(py + 1));
            const float rowCov = rowHi - rowLo;
            if (rowCov <= 0.0f) {
                continue;
            }
            if (useGrad) {
                // World space is affine in the device x, so walk it with two adds per pixel rather
                // than re-solving the inverse transform. For a LINEAR gradient the ramp parameter is
                // affine too, so it becomes one add and a table lookup -- no square root at all.
                float wx = 0.0f, wy = 0.0f;
                if (!toWorld(static_cast<float>(left) + 0.5f, static_cast<float>(py) + 0.5f, wx, wy)) {
                    continue;
                }
                const bool linear = !grad.radial;
                float t = 0.0f, dt = 0.0f;
                if (linear) {
                    const float gx = grad.x1 - grad.x0;
                    const float gy = grad.y1 - grad.y0;
                    const float len2 = gx * gx + gy * gy;
                    if (len2 > 0.0f) {
                        t = ((wx - grad.x0) * gx + (wy - grad.y0) * gy) / len2;
                        dt = (m_ia * gx + m_ib * gy) / len2;
                    }
                }
                for (int px = left; px < right; ++px) {
                    const float colLo = std::fmax(dx0, static_cast<float>(px));
                    const float colHi = std::fmin(dx1, static_cast<float>(px + 1));
                    const float cov = (colHi - colLo) * rowCov;
                    if (cov > 0.0f) {
                        m_img.blendPixel(px, py, linear ? grad.atT(t) : grad.atT(grad.rawT(wx, wy)),
                                         cov);
                    }
                    wx += m_ia;
                    wy += m_ib;
                    t += dt;
                }
                continue;
            }
            // A flat rectangle splits into at most three runs: a part-covered pixel at each end and
            // a fully-covered middle. The middle is one span call, which hoists the colour
            // conversion out of the loop -- and that conversion, done per pixel, was most of the
            // cost of a full-frame fill.
            const int firstWhole = static_cast<int>(std::ceil(dx0));
            const int lastWhole = static_cast<int>(std::floor(dx1));
            if (left < std::min(firstWhole, right)) {
                const float colHi = std::fmin(dx1, static_cast<float>(left + 1));
                m_img.blendSpan(left, left + 1, py, flat,
                                (colHi - std::fmax(dx0, static_cast<float>(left))) * rowCov);
            }
            const int midLo = std::max(left, firstWhole);
            const int midHi = std::min(right, lastWhole);
            if (midHi > midLo) {
                m_img.blendSpan(midLo, midHi, py, flat, rowCov);
            }
            if (lastWhole >= midLo && lastWhole < right && lastWhole >= left) {
                const float colLo = std::fmax(dx0, static_cast<float>(lastWhole));
                m_img.blendSpan(lastWhole, lastWhole + 1, py, flat,
                                (std::fmin(dx1, static_cast<float>(lastWhole + 1)) - colLo) * rowCov);
            }
        }
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

    // A partial arc, appended to the path as line segments. Angles follow canvas: 0 points right,
    // increasing angle turns toward +y (which is DOWN here, so clockwise on screen), and when the end
    // angle is behind the start one a full turn is added -- so arc(.., PI, 0) sweeps the top half
    // rather than drawing nothing, the way the sets rely on.
    void arcTo(float cx, float cy, float r, float a0, float a1) {
        float end = a1;
        while (end < a0) {
            end += 6.28318531f;
        }
        const float span = end - a0;
        // One segment per ~0.12 rad keeps a stroked arc smooth at any frame size the films use.
        int steps = static_cast<int>(std::ceil(span / 0.12f));
        if (steps < 2) {
            steps = 2;
        }
        for (int i = 0; i <= steps; ++i) {
            const float a = a0 + span * static_cast<float>(i) / static_cast<float>(steps);
            const float x = cx + r * std::cos(a);
            const float y = cy + r * std::sin(a);
            if (i == 0 && m_path.empty()) {
                m_path.moveTo(x, y);
            } else if (i == 0) {
                m_path.lineTo(x, y);
            } else {
                m_path.lineTo(x, y);
            }
        }
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

    // World space behind a device pixel, through the inverse cached by setTransform.
    bool toWorld(float dx, float dy, float& ox, float& oy) const {
        if (!m_invertible) {
            return false;
        }
        const float px = dx - m_e;
        const float py = dy - m_f;
        ox = px * m_ia + py * m_ic;
        oy = px * m_ib + py * m_id;
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
        render::FillClip clip;
        clip.left = left;
        clip.right = right;
        clip.top = top;
        clip.bottom = bottom;

        if (!m_useGradient) {
            const render::Color flat = m_fill;
            render::fillPathShaded(m_img, device, [&flat](int, int) -> const render::Color& {
                return flat;
            }, rule, 16, &flat, clip);
            return;
        }
        const Gradient grad = m_gradient;
        render::fillPathShaded(
            m_img, device,
            [&](int x, int y) -> render::Color {
                float wx = 0.0f, wy = 0.0f;
                if (!toWorld(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, wx, wy)) {
                    return render::Color{0.0f, 0.0f, 0.0f, 0.0f};
                }
                return grad.atT(grad.rawT(wx, wy));
            },
            rule, 16, nullptr, clip);
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
    float m_ia = 1.0f, m_ib = 0.0f, m_ic = 0.0f, m_id = 1.0f;
    bool m_invertible = true;
};

} // namespace maz::film
