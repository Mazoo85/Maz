#pragma once

#include "maz/math/Math.hpp" // math::vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render 2D SHAPE OUTLINES — ready-made point rings for the common flat shapes, so you don't hand-type
// coordinates. Each function returns a counter-clockwise list of 2D points tracing the outline of a shape, which
// you feed straight into `extrudePolygon` (M602) to make a 3D prism, `revolveProfile` (M594) to spin a solid,
// `triangulatePolygon` (M156) to fill it flat, or a 2D polygon collider. Between them they cover most of what UI,
// signage, and props need: a regular n-gon (hexagon nut, pentagon, octagon stop-sign), a star or sparkle, a
// rounded rectangle (button, card, badge, panel, rounded platform), and a spur gear / cog (machinery, clocks,
// steampunk). Header-only, deterministic, headless — pure coordinate math.
//
// Scope note (honest): all outlines are simple (non-self-intersecting) and wound COUNTER-CLOCKWISE, centred on the
// origin. A star with a big enough inner radius stays simple; a rounded rect clamps its corner radius to at most
// half the shorter side. These are OUTLINES (point rings), not filled meshes — pair them with the extrude/fill/
// revolve tools above.
namespace maz::render::shapes2d {

// A regular polygon: `sides` points evenly spaced on a circle of `radius`. `startAngle` rotates it (radians).
inline std::vector<math::vec2> regularPolygon(int sides, float radius, float startAngle = 0.0f) {
    std::vector<math::vec2> out;
    if (sides < 3 || radius <= 0.0f) return out;
    const float twoPi = 6.28318530717958647692f;
    out.reserve(static_cast<std::size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = startAngle + twoPi * static_cast<float>(i) / static_cast<float>(sides);
        out.push_back(math::vec2(std::cos(a) * radius, std::sin(a) * radius));
    }
    return out;
}

// A star: `points` tips at `outerRadius` alternating with `points` valleys at `innerRadius` (2*points vertices).
inline std::vector<math::vec2> star(int points, float outerRadius, float innerRadius) {
    std::vector<math::vec2> out;
    if (points < 2 || outerRadius <= 0.0f || innerRadius <= 0.0f) return out;
    const float pi = 3.14159265358979323846f;
    const int n = points * 2;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float a = pi * static_cast<float>(i) / static_cast<float>(points) + pi * 0.5f; // first tip points up
        const float r = (i % 2 == 0) ? outerRadius : innerRadius;
        out.push_back(math::vec2(std::cos(a) * r, std::sin(a) * r));
    }
    return out;
}

// A rounded rectangle of the given full width/height, corners rounded by `radius` with `cornerSegments` steps each.
inline std::vector<math::vec2> roundedRect(float width, float height, float radius, int cornerSegments = 4) {
    std::vector<math::vec2> out;
    if (width <= 0.0f || height <= 0.0f) return out;
    const float hw = width * 0.5f, hh = height * 0.5f;
    float r = radius;
    const float rmax = (hw < hh ? hw : hh);
    if (r > rmax) r = rmax;
    if (r <= 1e-6f) { // plain rectangle
        out = {math::vec2(-hw, -hh), math::vec2(hw, -hh), math::vec2(hw, hh), math::vec2(-hw, hh)};
        return out;
    }
    const int seg = cornerSegments < 1 ? 1 : cornerSegments;
    const float ix = hw - r, iy = hh - r; // corner-arc centres
    const float halfPi = 1.57079632679489661923f;
    // Four corners, CCW: bottom-right, top-right, top-left, bottom-left; each arc sweeps 90 degrees.
    const math::vec2 centres[4] = {math::vec2(ix, -iy), math::vec2(ix, iy), math::vec2(-ix, iy), math::vec2(-ix, -iy)};
    const float starts[4] = {-halfPi, 0.0f, halfPi, 2.0f * halfPi};
    out.reserve(static_cast<std::size_t>(4 * (seg + 1)));
    for (int c = 0; c < 4; ++c) {
        for (int s = 0; s <= seg; ++s) {
            const float a = starts[c] + halfPi * static_cast<float>(s) / static_cast<float>(seg);
            out.push_back(math::vec2(centres[c].x + std::cos(a) * r, centres[c].y + std::sin(a) * r));
        }
    }
    return out;
}

// A spur-gear / cog outline: `teeth` trapezoidal teeth rising from a root circle (`rootRadius`) to a tip circle
// (`outerRadius`). `toothWidthFrac` (0..1) is the fraction of each tooth-slot the tip occupies (0.5 = tip and gap
// equal). Returns 5 points per tooth (valley, rising flank base, tip-left, tip-right, falling flank base), CCW.
inline std::vector<math::vec2> gear(int teeth, float outerRadius, float rootRadius, float toothWidthFrac = 0.5f) {
    std::vector<math::vec2> out;
    if (teeth < 3 || outerRadius <= 0.0f || rootRadius <= 0.0f || rootRadius >= outerRadius) return out;
    float frac = toothWidthFrac;
    if (frac < 0.05f) frac = 0.05f;
    if (frac > 0.95f) frac = 0.95f;
    const float twoPi = 6.28318530717958647692f;
    const float step = twoPi / static_cast<float>(teeth);
    const float ht = step * frac * 0.5f;     // half the tip's angular width
    const float half = step * 0.5f;          // half a tooth-slot
    out.reserve(static_cast<std::size_t>(teeth) * 5u);
    auto at = [&](float r, float a) { out.push_back(math::vec2(std::cos(a) * r, std::sin(a) * r)); };
    for (int i = 0; i < teeth; ++i) {
        const float c = step * static_cast<float>(i);
        at(rootRadius, c - half);  // valley start (root)
        at(rootRadius, c - ht);    // base of the rising flank
        at(outerRadius, c - ht);   // tip left
        at(outerRadius, c + ht);   // tip right
        at(rootRadius, c + ht);    // base of the falling flank
    }
    return out;
}

} // namespace maz::render::shapes2d
