#pragma once

#include "maz/math/Math.hpp" // math::vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render::Path — a 2D vector path, the shape half of Godot's Geometry2D/CanvasItem drawing. The
// engine's CPU raster could draw a line, a rectangle, a circle outline and a single triangle; anything
// with a curve in it, a hole in it, or more than three corners had nowhere to go. A Path collects
// straight and curved segments (moveTo/lineTo/quadTo/cubicTo/rect/ellipse/close) and flattens the
// curves into polygon CONTOURS at a stated tolerance, so the rasterizer downstream only ever sees line
// segments. Multiple contours in one path is what gives shapes holes.
//
// Coordinates are in pixels with a TOP-LEFT origin (y grows downward), matching render::Image.
// Pure geometry — no image, no renderer, no allocation beyond the contour storage — so it is
// deterministic and unit-testable headlessly.
namespace maz::render {

class Path {
public:
    // Flattening tolerance in pixels: the furthest a flattened segment may stray from the true curve.
    // A twentieth of a pixel is below what an 8-bit raster can show, while keeping segment counts low.
    static constexpr float kDefaultTolerance = 0.05f;

    // --- building ---------------------------------------------------------------------------------

    // Start a new contour at (x,y).
    Path& moveTo(float x, float y) {
        m_contours.emplace_back();
        m_contours.back().push_back(math::vec2{x, y});
        m_open = true;
        return *this;
    }

    // Straight segment from the current point. With no current point this starts a contour instead.
    Path& lineTo(float x, float y) {
        if (!m_open || m_contours.empty()) {
            return moveTo(x, y);
        }
        m_contours.back().push_back(math::vec2{x, y});
        return *this;
    }

    // Quadratic Bezier from the current point, via control (cx,cy), to (x,y).
    Path& quadTo(float cx, float cy, float x, float y) {
        if (!m_open || m_contours.empty()) {
            moveTo(cx, cy);
        }
        const math::vec2 p0 = current();
        const math::vec2 p1{cx, cy};
        const math::vec2 p2{x, y};
        // B''(t) is the constant 2*(p0 - 2*p1 + p2), so an n-segment chord approximation strays by at
        // most |p0 - 2*p1 + p2| / (4*n^2). Solve that for n.
        const float m = length(p0 - 2.0f * p1 + p2);
        const int n = segmentsFor(m * 0.25f);
        for (int i = 1; i <= n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            const float mt = 1.0f - t;
            m_contours.back().push_back(mt * mt * p0 + 2.0f * mt * t * p1 + t * t * p2);
        }
        return *this;
    }

    // Cubic Bezier from the current point, via two controls, to (x,y).
    Path& cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) {
        if (!m_open || m_contours.empty()) {
            moveTo(c1x, c1y);
        }
        const math::vec2 p0 = current();
        const math::vec2 p1{c1x, c1y};
        const math::vec2 p2{c2x, c2y};
        const math::vec2 p3{x, y};
        // max|B''| <= 6*max(|p0-2p1+p2|, |p1-2p2+p3|), and the chord error is max|B''| / (8*n^2).
        const float m = std::fmax(length(p0 - 2.0f * p1 + p2), length(p1 - 2.0f * p2 + p3));
        const int n = segmentsFor(m * 0.75f);
        for (int i = 1; i <= n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            const float mt = 1.0f - t;
            m_contours.back().push_back(mt * mt * mt * p0 + 3.0f * mt * mt * t * p1 +
                                        3.0f * mt * t * t * p2 + t * t * t * p3);
        }
        return *this;
    }

    // A rectangle as its own closed contour. Negative extents are normalised.
    Path& rect(float x, float y, float w, float h) {
        if (w < 0.0f) { x += w; w = -w; }
        if (h < 0.0f) { y += h; h = -h; }
        moveTo(x, y).lineTo(x + w, y).lineTo(x + w, y + h).lineTo(x, y + h);
        return close();
    }

    // An ellipse as its own closed contour, optionally rotated, optionally wound the other way.
    //
    // Winding matters: a path's contours combine under the nonzero rule by winding direction, so an
    // inner contour wound OPPOSITE to its outer one cuts a hole, while one wound the same way does not.
    // That is how a ring, a letter 'O' or an eye socket gets its hole.
    Path& ellipse(float cx, float cy, float rx, float ry, float rotation = 0.0f,
                  bool clockwise = false) {
        rx = std::fabs(rx);
        ry = std::fabs(ry);
        if (rx <= 0.0f || ry <= 0.0f) {
            return *this;
        }
        // A chord spanning angle theta on a circle of radius r bulges away from the curve by
        // r*(1-cos(theta/2)) ~= r*theta^2/8. Solve for theta at the tolerance, on the larger radius.
        const float r = std::fmax(rx, ry);
        const float theta = std::sqrt(8.0f * m_tolerance / r);
        int n = static_cast<int>(std::ceil(6.28318530718f / theta));
        n = n < 8 ? 8 : (n > 4096 ? 4096 : n);

        // Vertices placed exactly on the ellipse give a polygon that is entirely INSIDE it, so every
        // ellipse would come out systematically a little small — a shrunken head, a shy shadow. Pushing
        // the radius out by half the bulge centres the error on the true ellipse instead of biasing it
        // inward, which costs nothing and is what makes filled area match pi*rx*ry.
        const float half = 3.14159265359f / static_cast<float>(n);
        const float grow = 1.0f + (1.0f - std::cos(half)) * 0.5f;
        const float ax = rx * grow;
        const float by = ry * grow;

        const float cosR = std::cos(rotation);
        const float sinR = std::sin(rotation);
        const float step = (clockwise ? -6.28318530718f : 6.28318530718f) / static_cast<float>(n);

        m_contours.emplace_back();
        auto& c = m_contours.back();
        c.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            const float a = step * static_cast<float>(i);
            const float ex = ax * std::cos(a);
            const float ey = by * std::sin(a);
            c.push_back(math::vec2{cx + ex * cosR - ey * sinR, cy + ex * sinR + ey * cosR});
        }
        m_open = false;
        return *this;
    }

    // Finish the current contour. Fills close every contour implicitly, so this only says that a
    // following lineTo starts a new contour rather than continuing this one.
    Path& close() {
        m_open = false;
        return *this;
    }

    void clear() {
        m_contours.clear();
        m_open = false;
    }

    // --- reading ----------------------------------------------------------------------------------

    bool empty() const { return m_contours.empty(); }
    const std::vector<std::vector<math::vec2>>& contours() const { return m_contours; }

    float tolerance() const { return m_tolerance; }
    // Applies to curves flattened AFTER this call; already-flattened contours keep their segments.
    void setTolerance(float t) { m_tolerance = t > 1e-5f ? t : 1e-5f; }

    // Axis-aligned bounds of every contour point. False (and untouched outputs) for an empty path.
    bool bounds(math::vec2& lo, math::vec2& hi) const {
        bool any = false;
        for (const auto& c : m_contours) {
            for (const auto& p : c) {
                if (!any) {
                    lo = p;
                    hi = p;
                    any = true;
                } else {
                    lo.x = std::fmin(lo.x, p.x);
                    lo.y = std::fmin(lo.y, p.y);
                    hi.x = std::fmax(hi.x, p.x);
                    hi.y = std::fmax(hi.y, p.y);
                }
            }
        }
        return any;
    }

private:
    math::vec2 current() const {
        return m_contours.empty() || m_contours.back().empty() ? math::vec2{0.0f, 0.0f}
                                                               : m_contours.back().back();
    }

    static float length(const math::vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

    // Segments needed so that `errNumerator / n^2` falls to the tolerance.
    int segmentsFor(float errNumerator) const {
        if (!(errNumerator > 0.0f)) {
            return 1;
        }
        const int n = static_cast<int>(std::ceil(std::sqrt(errNumerator / m_tolerance)));
        return n < 1 ? 1 : (n > 4096 ? 4096 : n);
    }

    std::vector<std::vector<math::vec2>> m_contours;
    float m_tolerance = kDefaultTolerance;
    bool m_open = false;
};

} // namespace maz::render
