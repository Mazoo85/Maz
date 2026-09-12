#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <vector>

// maz::math superellipse / squircle — the Lamé curve |x/a|^n + |y/b|^n = 1, a one-parameter family that
// morphs smoothly from a pinched astroid (n<1), through the ellipse (n=2), to a rounded "squircle" (n=4,
// the iOS-style rounded rectangle) and on toward a sharp rectangle (n→∞). Godot has no superellipse
// primitive; this fills the gap for smooth rounded-rectangle UI panels, organic blob shapes, camera/motion
// easing regions and procedural authoring. The parametric form used here places every returned point EXACTLY
// on the curve (|x/a|^n + |y/b|^n = 1 to floating-point), so the ring can be fed straight to the polygon
// fill / triangulator. Pure vec2 math, deterministic, header-only.
namespace maz::math {

// A single point on the superellipse with semi-axes (a, b) and exponent `n` (> 0), at angle parameter
// `t` (radians). Uses the standard parametric form x = a·sgn(cos t)·|cos t|^(2/n), y = b·sgn(sin t)·
// |sin t|^(2/n), which lies exactly on |x/a|^n + |y/b|^n = 1.
inline vec2 superellipsePoint(float a, float b, float n, float t) {
    const float ct = std::cos(t);
    const float st = std::sin(t);
    const float e = 2.0f / n;
    const float x = a * (ct < 0.0f ? -1.0f : 1.0f) * std::pow(std::fabs(ct), e);
    const float y = b * (st < 0.0f ? -1.0f : 1.0f) * std::pow(std::fabs(st), e);
    return vec2(x, y);
}

// A closed CCW ring of `segments` points sampling the superellipse once around (t = 0 .. 2π, excluding the
// duplicate endpoint). `segments` is clamped to at least 3.
inline std::vector<vec2> superellipsePolyline(float a, float b, float n, int segments) {
    if (segments < 3) {
        segments = 3;
    }
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(segments));
    const float twoPi = 6.28318530717958648f;
    for (int i = 0; i < segments; ++i) {
        const float t = twoPi * static_cast<float>(i) / static_cast<float>(segments);
        out.push_back(superellipsePoint(a, b, n, t));
    }
    return out;
}

// The squircle: the symmetric superellipse of exponent 4 with radius `r` (a = b = r) — the classic
// rounded-square used for app icons and modern UI panels.
inline std::vector<vec2> squircle(float r, int segments) {
    return superellipsePolyline(r, r, 4.0f, segments);
}

// Implicit test: true when point `p` lies inside (or on) the superellipse with semi-axes (a, b), exponent
// `n`, centred at the origin — i.e. |p.x/a|^n + |p.y/b|^n <= 1.
inline bool superellipseContains(float a, float b, float n, const vec2& p) {
    if (a <= 0.0f || b <= 0.0f) {
        return false;
    }
    const float v = std::pow(std::fabs(p.x / a), n) + std::pow(std::fabs(p.y / b), n);
    return v <= 1.0f + 1e-5f;
}

} // namespace maz::math
