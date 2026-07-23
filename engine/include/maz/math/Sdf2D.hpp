#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>

// maz::math analytic 2D signed distance functions — for each point, the exact distance to a shape's outline,
// NEGATIVE inside and positive outside. SDFs are the workhorse of crisp procedural 2D: resolution-independent
// UI shapes and icons, soft/glow/outline effects, dynamic masks, metaballs, 2D soft shadows, and analytic
// distance-based collision — all from a formula, no texture needed. The engine has a glyph-SDF *baker*
// (ui::Sdf) and a 3D SDF-CSG set (game::Csg); this is the missing library of exact 2D shape primitives (the
// Inigo Quilez collection). Shapes are centred at the origin in their own frame — translate/rotate the query
// point into local space before calling (sdSegment / sdOrientedBox / sdTriangle take explicit points). Godot
// exposes none of these. Every function returns a true distance field (unit gradient), so results compose
// with min (union) / max (intersection) / negation (subtraction). Header-only, std-only, deterministic.
namespace maz::math {

namespace detail2d {
inline float len2(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float dot2(const vec2& a, const vec2& b) { return a.x * b.x + a.y * b.y; }
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float sgn(float x) { return x < 0.0f ? -1.0f : (x > 0.0f ? 1.0f : 0.0f); }
} // namespace detail2d

// Circle of radius `r` centred at the origin.
inline float sdCircle(const vec2& p, float r) {
    return detail2d::len2(p) - r;
}

// Axis-aligned box with half-extents `b`, centred at the origin.
inline float sdBox(const vec2& p, const vec2& b) {
    const vec2 d(std::fabs(p.x) - b.x, std::fabs(p.y) - b.y);
    const vec2 dmax(d.x > 0.0f ? d.x : 0.0f, d.y > 0.0f ? d.y : 0.0f);
    return detail2d::len2(dmax) + std::min(std::max(d.x, d.y), 0.0f);
}

// Box with half-extents `b` and uniformly rounded corners of radius `r`.
inline float sdRoundedBox(const vec2& p, const vec2& b, float r) {
    const vec2 q(std::fabs(p.x) - b.x + r, std::fabs(p.y) - b.y + r);
    const vec2 qmax(q.x > 0.0f ? q.x : 0.0f, q.y > 0.0f ? q.y : 0.0f);
    return std::min(std::max(q.x, q.y), 0.0f) + detail2d::len2(qmax) - r;
}

// Distance to the line segment a–b (an unsigned distance field: 0 on the segment, unit gradient elsewhere).
inline float sdSegment(const vec2& p, const vec2& a, const vec2& b) {
    const vec2 pa = p - a, ba = b - a;
    const float h = detail2d::clampf(detail2d::dot2(pa, ba) / detail2d::dot2(ba, ba), 0.0f, 1.0f);
    return detail2d::len2(pa - ba * h);
}

// Oriented box: the rectangle of full thickness `thickness` centred on the segment a–b.
inline float sdOrientedBox(const vec2& p, const vec2& a, const vec2& b, float thickness) {
    const float l = detail2d::len2(b - a);
    if (l < 1e-9f) {
        return detail2d::len2(p - a) - thickness * 0.5f;
    }
    const vec2 d = (b - a) * (1.0f / l);
    const vec2 q0 = p - (a + b) * 0.5f;
    const vec2 q(d.x * q0.x + d.y * q0.y, -d.y * q0.x + d.x * q0.y); // rotate into the box frame
    const vec2 qa(std::fabs(q.x) - l * 0.5f, std::fabs(q.y) - thickness * 0.5f);
    const vec2 qmax(qa.x > 0.0f ? qa.x : 0.0f, qa.y > 0.0f ? qa.y : 0.0f);
    return detail2d::len2(qmax) + std::min(std::max(qa.x, qa.y), 0.0f);
}

// Equilateral triangle of "radius" `r`, pointing up, centred at the origin.
inline float sdEquilateralTriangle(const vec2& p, float r) {
    const float k = 1.73205081f; // sqrt(3)
    vec2 q = p;
    q.x = std::fabs(q.x) - r;
    q.y = q.y + r / k;
    if (q.x + k * q.y > 0.0f) {
        q = vec2(q.x - k * q.y, -k * q.x - q.y) * 0.5f;
    }
    q.x -= detail2d::clampf(q.x, -2.0f * r, 0.0f);
    return -detail2d::len2(q) * detail2d::sgn(q.y);
}

// Arbitrary triangle with vertices p0, p1, p2 (winding-independent exact signed distance).
inline float sdTriangle(const vec2& p, const vec2& p0, const vec2& p1, const vec2& p2) {
    using namespace detail2d;
    const vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    const vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    const vec2 pq0 = v0 - e0 * clampf(dot2(v0, e0) / dot2(e0, e0), 0.0f, 1.0f);
    const vec2 pq1 = v1 - e1 * clampf(dot2(v1, e1) / dot2(e1, e1), 0.0f, 1.0f);
    const vec2 pq2 = v2 - e2 * clampf(dot2(v2, e2) / dot2(e2, e2), 0.0f, 1.0f);
    const float s = sgn(e0.x * e2.y - e0.y * e2.x);
    float dx = std::min(std::min(dot2(pq0, pq0), dot2(pq1, pq1)), dot2(pq2, pq2));
    float dy = std::min(std::min(s * (v0.x * e0.y - v0.y * e0.x), s * (v1.x * e1.y - v1.y * e1.x)),
                        s * (v2.x * e2.y - v2.y * e2.x));
    return -std::sqrt(dx) * sgn(dy);
}

// Regular hexagon of inradius (apothem) `r`, centred at the origin.
inline float sdHexagon(const vec2& p, float r) {
    const float kx = -0.866025404f, ky = 0.5f, kz = 0.577350269f;
    float px = std::fabs(p.x), py = std::fabs(p.y);
    const float d = std::min(kx * px + ky * py, 0.0f);
    px -= 2.0f * d * kx;
    py -= 2.0f * d * ky;
    px -= detail2d::clampf(px, -kz * r, kz * r);
    py -= r;
    return detail2d::len2(vec2(px, py)) * detail2d::sgn(py);
}

// Pie / circular sector of radius `r` and HALF-aperture `halfAngle` (radians), pointing up (+y), apex at the
// origin. halfAngle = π gives the full disk.
inline float sdPie(const vec2& p, float r, float halfAngle) {
    const vec2 c(std::sin(halfAngle), std::cos(halfAngle));
    vec2 q(std::fabs(p.x), p.y);
    const float l = detail2d::len2(q) - r;
    const vec2 proj = c * detail2d::clampf(detail2d::dot2(q, c), 0.0f, r);
    const float m = detail2d::len2(q - proj);
    return std::max(l, m * detail2d::sgn(c.y * q.x - c.x * q.y));
}

} // namespace maz::math
