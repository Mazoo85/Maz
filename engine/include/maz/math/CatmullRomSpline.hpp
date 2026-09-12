#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math centripetal Catmull-Rom spline — a smooth curve that passes THROUGH a list of control
// points, the standard tool for camera rails, roads/rivers, and enemy patrol paths laid out as
// waypoints. The engine already has a single uniform Catmull-Rom segment (VectorOps cubicInterpolate),
// but uniform parameterization famously produces cusps and self-intersecting loops when waypoints are
// unevenly spaced or turn sharply. The CENTRIPETAL variant (alpha = 0.5), from Yuksel et al., spaces
// the knots by the square-root of the distance between points, which provably removes those cusps and
// loops while still interpolating every control point exactly. This is the chain-of-segments form with
// selectable parameterization (0 = uniform, 0.5 = centripetal, 1 = chordal). Pure vec2 math, no
// allocation per eval — deterministic and exactly unit-testable (it hits each control point on the
// dot).
namespace maz::math {

namespace detail {
// Linear blend between two knots a@ta and b@tb evaluated at t (ta != tb guaranteed by the caller).
inline vec2 knotLerp(const vec2& a, const vec2& b, float ta, float tb, float t) {
    const float w = (t - ta) / (tb - ta);
    return vec2(a.x + (b.x - a.x) * w, a.y + (b.y - a.y) * w);
}
} // namespace detail

struct CatmullRomSpline {
    std::vector<vec2> points;
    float alpha = 0.5f; // 0 = uniform, 0.5 = centripetal (default), 1 = chordal

    std::size_t size() const { return points.size(); }

    // Evaluate one segment defined by p0..p3 at local parameter s in [0,1]; the curve runs p1 -> p2.
    vec2 segment(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float s) const {
        auto knot = [&](float t, const vec2& a, const vec2& b) {
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            float k = std::pow(std::sqrt(dx * dx + dy * dy), alpha);
            if (k < 1e-6f) {
                k = 1e-6f; // guard coincident points (degenerate knot) so we never divide by zero
            }
            return t + k;
        };
        const float t0 = 0.0f;
        const float t1 = knot(t0, p0, p1);
        const float t2 = knot(t1, p1, p2);
        const float t3 = knot(t2, p2, p3);
        const float t = t1 + s * (t2 - t1);

        const vec2 a1 = detail::knotLerp(p0, p1, t0, t1, t);
        const vec2 a2 = detail::knotLerp(p1, p2, t1, t2, t);
        const vec2 a3 = detail::knotLerp(p2, p3, t2, t3, t);
        const vec2 b1 = detail::knotLerp(a1, a2, t0, t2, t);
        const vec2 b2 = detail::knotLerp(a2, a3, t1, t3, t);
        return detail::knotLerp(b1, b2, t1, t2, t);
    }

    // Evaluate the whole spline at parameter u in [0, size()-1]. u == i returns points[i] exactly.
    vec2 eval(float u) const {
        const int n = static_cast<int>(points.size());
        if (n == 0) {
            return vec2(0.0f, 0.0f);
        }
        if (n == 1) {
            return points[0];
        }
        const float maxU = static_cast<float>(n - 1);
        if (u <= 0.0f) {
            u = 0.0f;
        }
        if (u >= maxU) {
            u = maxU;
        }
        int i = static_cast<int>(u);
        if (i >= n - 1) {
            i = n - 2;
        }
        const float s = u - static_cast<float>(i);
        // Endpoints duplicate the boundary control point (clamped tangents).
        const vec2& p0 = points[static_cast<std::size_t>(i > 0 ? i - 1 : 0)];
        const vec2& p1 = points[static_cast<std::size_t>(i)];
        const vec2& p2 = points[static_cast<std::size_t>(i + 1)];
        const vec2& p3 = points[static_cast<std::size_t>(i + 2 < n ? i + 2 : n - 1)];
        return segment(p0, p1, p2, p3, s);
    }

    // Sample the whole spline into `samplesPerSegment` points per segment (plus the final endpoint).
    std::vector<vec2> tessellate(int samplesPerSegment) const {
        std::vector<vec2> out;
        const int n = static_cast<int>(points.size());
        if (n == 0 || samplesPerSegment < 1) {
            return out;
        }
        if (n == 1) {
            out.push_back(points[0]);
            return out;
        }
        const int segs = n - 1;
        for (int seg = 0; seg < segs; ++seg) {
            for (int k = 0; k < samplesPerSegment; ++k) {
                const float s = static_cast<float>(k) / static_cast<float>(samplesPerSegment);
                out.push_back(eval(static_cast<float>(seg) + s));
            }
        }
        out.push_back(points[static_cast<std::size_t>(n - 1)]);
        return out;
    }
};

} // namespace maz::math
