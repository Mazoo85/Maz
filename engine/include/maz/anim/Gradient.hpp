#pragma once

#include "maz/render/Renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace maz::anim {

// Gradient — Godot's Gradient resource: a colour ramp defined by sorted (offset, colour) stops and sampled
// over a parameter (usually [0,1]). It is the colour analogue of `anim::Curve` (which maps a scalar), and it
// drives colour-over-lifetime for particles, health/heat tints, sky ramps, minimap legends, and bakes into a
// GradientTexture. Three interpolation modes match Godot's Gradient.InterpolationMode: Constant (hard bands —
// the lower stop's colour holds until the next), Linear (straight per-channel blend), and Cubic (a Catmull-Rom
// spline through the neighbouring stops for smooth, slightly overshooting transitions). Below the first stop it
// returns the first colour, above the last the last colour (Godot's clamped domain). Pure data + math,
// header-only, deterministic — it unit-tests exactly and drives a golden.

enum class GradientInterp { Constant, Linear, Cubic };

struct GradientStop {
    float offset = 0.0f;                    // position along the ramp, kept sorted ascending (usually [0,1])
    render::Color color{0.0f, 0.0f, 0.0f, 1.0f};
};

class Gradient {
public:
    GradientInterp interp = GradientInterp::Linear;

    Gradient() = default;

    // Two-stop convenience (Godot's default is a black→white ramp).
    Gradient(render::Color a, render::Color b) {
        addStop(0.0f, a);
        addStop(1.0f, b);
    }

    // Insert a stop, keeping the list sorted by offset.
    void addStop(float offset, render::Color color) {
        const GradientStop s{offset, color};
        auto it = std::lower_bound(m_stops.begin(), m_stops.end(), offset,
                                   [](const GradientStop& a, float x) { return a.offset < x; });
        m_stops.insert(it, s);
    }

    void clear() { m_stops.clear(); }
    std::size_t stopCount() const { return m_stops.size(); }
    const GradientStop& stop(std::size_t i) const { return m_stops[i]; }

    void setColor(std::size_t i, render::Color c) {
        if (i < m_stops.size()) {
            m_stops[i].color = c;
        }
    }

    // Move a stop to a new offset (re-sorts, so the index may change).
    void setOffset(std::size_t i, float off) {
        if (i >= m_stops.size()) {
            return;
        }
        const render::Color c = m_stops[i].color;
        m_stops.erase(m_stops.begin() + static_cast<std::ptrdiff_t>(i));
        addStop(off, c);
    }

    // Sample the ramp at `t`. Below the first stop → first colour; above the last → last colour.
    render::Color sample(float t) const {
        if (m_stops.empty()) {
            return render::Color{0.0f, 0.0f, 0.0f, 1.0f};
        }
        if (m_stops.size() == 1 || t <= m_stops.front().offset) {
            return m_stops.front().color;
        }
        if (t >= m_stops.back().offset) {
            return m_stops.back().color;
        }
        // Find the segment [i0, i1] with i0.offset <= t < i1.offset.
        std::size_t i = 1;
        while (i < m_stops.size() && m_stops[i].offset <= t) {
            ++i;
        }
        const std::size_t i0 = i - 1;
        const std::size_t i1 = i;
        const GradientStop& a = m_stops[i0];
        const GradientStop& b = m_stops[i1];
        const float d = b.offset - a.offset;
        if (d <= 1e-9f) {
            return b.color;
        }
        const float u = (t - a.offset) / d;

        if (interp == GradientInterp::Constant) {
            return a.color;
        }
        if (interp == GradientInterp::Linear) {
            return lerpColor(a.color, b.color, u);
        }
        // Cubic: Catmull-Rom through the four surrounding stops (neighbours clamped at the ends).
        const render::Color p0 = m_stops[i0 == 0 ? 0 : i0 - 1].color;
        const render::Color p1 = a.color;
        const render::Color p2 = b.color;
        const render::Color p3 = m_stops[i1 + 1 < m_stops.size() ? i1 + 1 : i1].color;
        return catmullRom(p0, p1, p2, p3, u);
    }

    // Bake into an N-sample ramp (Godot's GradientTexture1D): evenly sampled across [0,1].
    std::vector<render::Color> bake(std::size_t count) const {
        std::vector<render::Color> out;
        if (count == 0) {
            return out;
        }
        out.reserve(count);
        if (count == 1) {
            out.push_back(sample(0.5f));
            return out;
        }
        for (std::size_t i = 0; i < count; ++i) {
            out.push_back(sample(static_cast<float>(i) / static_cast<float>(count - 1)));
        }
        return out;
    }

private:
    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    static render::Color lerpColor(render::Color a, render::Color b, float u) {
        return render::Color{a.r + (b.r - a.r) * u, a.g + (b.g - a.g) * u, a.b + (b.b - a.b) * u,
                             a.a + (b.a - a.a) * u};
    }

    static float catmull(float p0, float p1, float p2, float p3, float u) {
        const float u2 = u * u;
        const float u3 = u2 * u;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u3);
    }

    static render::Color catmullRom(render::Color p0, render::Color p1, render::Color p2, render::Color p3,
                                    float u) {
        return render::Color{clamp01(catmull(p0.r, p1.r, p2.r, p3.r, u)),
                             clamp01(catmull(p0.g, p1.g, p2.g, p3.g, u)),
                             clamp01(catmull(p0.b, p1.b, p2.b, p3.b, u)),
                             clamp01(catmull(p0.a, p1.a, p2.a, p3.a, u))};
    }

    std::vector<GradientStop> m_stops;
};

} // namespace maz::anim
