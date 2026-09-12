#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace maz::anim {

// Curve — Godot's Curve resource: a keyframed 1-D function y = f(x), sampled over a domain (usually [0,1]),
// that drives value-over-time / value-over-parameter effects — particle size or alpha over lifetime, an
// audio fade, a custom easing shape, a difficulty ramp. This is NOT math::Curve2D (a Bézier *path* through
// 2D space); this maps one scalar to another. Points carry per-point left/right TANGENTS (slopes) so the
// Cubic mode is a smooth Hermite spline; Linear and Constant modes ignore tangents. Results are clamped to
// [minValue, maxValue]. Pure math, header-only, deterministic — it unit-tests exactly and drives a golden.

enum class CurveInterp { Constant, Linear, Cubic };

struct CurvePoint {
    float pos = 0.0f;          // x (domain), points kept sorted ascending
    float value = 0.0f;        // y
    float leftTangent = 0.0f;  // slope entering this point (Cubic mode)
    float rightTangent = 0.0f; // slope leaving this point (Cubic mode)
};

class Curve {
public:
    CurveInterp interp = CurveInterp::Cubic;
    float minValue = 0.0f;
    float maxValue = 1.0f;

    // Insert a point, keeping the list sorted by position.
    void addPoint(float pos, float value, float leftTangent = 0.0f, float rightTangent = 0.0f) {
        CurvePoint p{pos, value, leftTangent, rightTangent};
        auto it = std::lower_bound(m_points.begin(), m_points.end(), pos,
                                   [](const CurvePoint& a, float x) { return a.pos < x; });
        m_points.insert(it, p);
    }

    void clear() { m_points.clear(); }
    std::size_t pointCount() const { return m_points.size(); }
    const CurvePoint& point(std::size_t i) const { return m_points[i]; }

    // Sample the curve at `x`. Below the first point returns the first value; above the last returns the
    // last value (Godot's clamped domain). The result is clamped to [minValue, maxValue].
    float sample(float x) const {
        if (m_points.empty()) {
            return clampValue(0.0f);
        }
        if (m_points.size() == 1 || x <= m_points.front().pos) {
            return clampValue(m_points.front().value);
        }
        if (x >= m_points.back().pos) {
            return clampValue(m_points.back().value);
        }
        // Find the segment [a, b] with a.pos <= x < b.pos.
        std::size_t i = 1;
        while (i < m_points.size() && m_points[i].pos <= x) {
            ++i;
        }
        const CurvePoint& a = m_points[i - 1];
        const CurvePoint& b = m_points[i];
        const float d = b.pos - a.pos;
        if (d <= 1e-9f) {
            return clampValue(b.value);
        }
        const float t = (x - a.pos) / d;

        float y;
        if (interp == CurveInterp::Constant) {
            y = a.value;
        } else if (interp == CurveInterp::Linear) {
            y = a.value + (b.value - a.value) * t;
        } else {
            // Cubic Hermite using the segment's endpoint tangents (slopes scaled by the segment width).
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h1 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h2 = -2.0f * t3 + 3.0f * t2;
            const float h3 = t3 - 2.0f * t2 + t;
            const float h4 = t3 - t2;
            y = h1 * a.value + h2 * b.value + h3 * (a.rightTangent * d) + h4 * (b.leftTangent * d);
        }
        return clampValue(y);
    }

private:
    float clampValue(float v) const {
        return v < minValue ? minValue : (v > maxValue ? maxValue : v);
    }

    std::vector<CurvePoint> m_points;
};

} // namespace maz::anim
