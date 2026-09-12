#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::math {

// Cubic Bézier path — Godot's Curve2D / the spline a Path2D holds and a PathFollow2D walks. Maz had easing
// curves (anim) for scalar interpolation, but no *spatial* path: an authored smooth curve through a set of
// points that something can travel along at constant speed. That is what Curve2D provides. Each point carries
// a position plus `in`/`out` control handles (offsets relative to the point, exactly like Godot's Curve2D),
// and consecutive points are joined by a cubic Bézier. `sample`/`tangent` evaluate the geometric curve;
// `bake` walks it and lays down points spaced evenly by ARC LENGTH, so `sampleBaked(distance)` moves along
// the path at uniform speed (naive Bézier `t` bunches up where the curve bends). Header-only, math-only (no
// renderer), so it unit-tests headlessly; the app draws the curve, its handles, and the constant-speed points.

struct CurvePoint2D {
    vec2 position{0.0f, 0.0f};
    vec2 in{0.0f, 0.0f};  // control handle leading INTO the point (relative offset)
    vec2 out{0.0f, 0.0f}; // control handle leading OUT of the point (relative offset)
};

namespace detail {

inline vec2 cubicBezier(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t) {
    const float u = 1.0f - t;
    const float uu = u * u;
    const float tt = t * t;
    return uu * u * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + tt * t * p3;
}

} // namespace detail

class Curve2D {
public:
    void addPoint(const vec2& position, const vec2& in = vec2(0.0f), const vec2& out = vec2(0.0f)) {
        m_points.push_back({position, in, out});
        m_baked.clear();
        m_bakedDist.clear();
    }

    void clear() {
        m_points.clear();
        m_baked.clear();
        m_bakedDist.clear();
        m_bakedLength = 0.0f;
    }

    std::size_t pointCount() const { return m_points.size(); }
    const CurvePoint2D& point(std::size_t i) const { return m_points[i]; }

    // Evaluate the cubic Bézier of segment `seg` (point seg -> seg+1) at local t in [0,1].
    vec2 sampleSegment(std::size_t seg, float t) const {
        if (m_points.empty()) {
            return vec2(0.0f);
        }
        if (m_points.size() == 1) {
            return m_points[0].position;
        }
        if (seg + 1 >= m_points.size()) {
            seg = m_points.size() - 2;
        }
        const CurvePoint2D& a = m_points[seg];
        const CurvePoint2D& b = m_points[seg + 1];
        return detail::cubicBezier(a.position, a.position + a.out, b.position + b.in, b.position, t);
    }

    // Evaluate at a fractional point offset in [0, pointCount-1] (Godot's samplef): the integer part picks
    // the segment, the fraction is t within it.
    vec2 sample(float fofs) const {
        if (m_points.empty()) {
            return vec2(0.0f);
        }
        if (m_points.size() == 1) {
            return m_points[0].position;
        }
        const float maxOfs = static_cast<float>(m_points.size() - 1);
        if (fofs <= 0.0f) {
            return m_points.front().position;
        }
        if (fofs >= maxOfs) {
            return m_points.back().position;
        }
        const float segf = std::floor(fofs);
        const std::size_t seg = static_cast<std::size_t>(segf);
        return sampleSegment(seg, fofs - segf);
    }

    // Unit tangent (direction of travel) at a fractional offset. Uses a central finite difference so it stays
    // well-defined even where the analytic Bézier derivative degenerates (e.g. a straight segment with no
    // handles, whose endpoint derivative is zero).
    vec2 tangent(float fofs) const {
        const float e = 1e-3f;
        const vec2 d = sample(fofs + e) - sample(fofs - e);
        const float len = glm::length(d);
        return len > 1e-6f ? d / len : vec2(1.0f, 0.0f);
    }

    // Approximate total arc length by summing chords of a fine subdivision.
    float length(int stepsPerSegment = 32) const {
        if (m_points.size() < 2 || stepsPerSegment < 1) {
            return 0.0f;
        }
        float total = 0.0f;
        vec2 prev = sampleSegment(0, 0.0f);
        for (std::size_t s = 0; s + 1 < m_points.size(); ++s) {
            for (int i = 1; i <= stepsPerSegment; ++i) {
                const vec2 p = sampleSegment(s, static_cast<float>(i) / static_cast<float>(stepsPerSegment));
                total += glm::length(p - prev);
                prev = p;
            }
        }
        return total;
    }

    // Lay down points spaced ~`interval` apart in arc length (constant-speed samples). Auto-called by
    // sampleBaked if not yet baked.
    void bake(float interval = 8.0f) {
        m_baked.clear();
        m_bakedDist.clear();
        m_bakedLength = 0.0f;
        if (interval <= 0.0f) {
            interval = 8.0f;
        }
        if (m_points.empty()) {
            return;
        }
        if (m_points.size() == 1) {
            m_baked.push_back(m_points[0].position);
            m_bakedDist.push_back(0.0f);
            return;
        }

        // Build a dense (point, cumulative-distance) polyline of the whole curve.
        const int steps = 64;
        std::vector<vec2> dense;
        std::vector<float> denseDist;
        dense.push_back(sampleSegment(0, 0.0f));
        denseDist.push_back(0.0f);
        float acc = 0.0f;
        for (std::size_t s = 0; s + 1 < m_points.size(); ++s) {
            for (int i = 1; i <= steps; ++i) {
                const vec2 p = sampleSegment(s, static_cast<float>(i) / static_cast<float>(steps));
                acc += glm::length(p - dense.back());
                dense.push_back(p);
                denseDist.push_back(acc);
            }
        }
        m_bakedLength = acc;

        // Resample the dense polyline at uniform arc-length intervals.
        std::size_t di = 0;
        for (float d = 0.0f; d <= acc + 1e-4f; d += interval) {
            while (di + 1 < denseDist.size() && denseDist[di + 1] < d) {
                ++di;
            }
            vec2 p;
            if (di + 1 >= dense.size()) {
                p = dense.back();
            } else {
                const float span = denseDist[di + 1] - denseDist[di];
                const float f = span > 1e-6f ? (d - denseDist[di]) / span : 0.0f;
                p = dense[di] + (dense[di + 1] - dense[di]) * f;
            }
            m_baked.push_back(p);
            m_bakedDist.push_back(d);
        }
        // Guarantee the exact endpoint is the last baked sample.
        if (m_bakedDist.empty() || m_bakedDist.back() < acc - 1e-3f) {
            m_baked.push_back(dense.back());
            m_bakedDist.push_back(acc);
        }
    }

    float bakedLength() const { return m_bakedLength; }
    const std::vector<vec2>& bakedPoints() const { return m_baked; }

    // Position at a given arc-length distance along the path (constant speed). Auto-bakes on first use.
    vec2 sampleBaked(float distance) {
        if (m_baked.empty()) {
            bake();
        }
        if (m_baked.empty()) {
            return vec2(0.0f);
        }
        if (m_baked.size() == 1 || distance <= 0.0f) {
            return m_baked.front();
        }
        if (distance >= m_bakedLength) {
            return m_baked.back();
        }
        std::size_t i = 0;
        while (i + 1 < m_bakedDist.size() && m_bakedDist[i + 1] < distance) {
            ++i;
        }
        const float span = m_bakedDist[i + 1] - m_bakedDist[i];
        const float f = span > 1e-6f ? (distance - m_bakedDist[i]) / span : 0.0f;
        return m_baked[i] + (m_baked[i + 1] - m_baked[i]) * f;
    }

    // Point on the baked path nearest to `toPoint` — Godot's Curve2D.get_closest_point. Auto-bakes.
    vec2 closestPoint(const vec2& toPoint) {
        if (m_baked.empty()) {
            bake();
        }
        if (m_baked.empty()) {
            return vec2(0.0f);
        }
        if (m_baked.size() == 1) {
            return m_baked.front();
        }
        vec2 best = m_baked.front();
        float bestD2 = distanceSquared(toPoint, best);
        for (std::size_t i = 0; i + 1 < m_baked.size(); ++i) {
            const vec2 p = closestOnSegment(toPoint, m_baked[i], m_baked[i + 1]);
            const float d2 = distanceSquared(toPoint, p);
            if (d2 < bestD2) {
                bestD2 = d2;
                best = p;
            }
        }
        return best;
    }

    // Arc-length offset along the path of the point nearest to `toPoint` — Godot's
    // Curve2D.get_closest_offset. Auto-bakes.
    float closestOffset(const vec2& toPoint) {
        if (m_baked.empty()) {
            bake();
        }
        if (m_baked.size() < 2) {
            return 0.0f;
        }
        float bestOffset = 0.0f;
        float bestD2 = distanceSquared(toPoint, m_baked.front());
        for (std::size_t i = 0; i + 1 < m_baked.size(); ++i) {
            const vec2 a = m_baked[i];
            const vec2 b = m_baked[i + 1];
            const vec2 ab = b - a;
            const float len2 = glm::dot(ab, ab);
            const float t = len2 > 1e-12f ? std::clamp(glm::dot(toPoint - a, ab) / len2, 0.0f, 1.0f)
                                          : 0.0f;
            const vec2 p = a + ab * t;
            const float d2 = distanceSquared(toPoint, p);
            if (d2 < bestD2) {
                bestD2 = d2;
                bestOffset = m_bakedDist[i] + t * (m_bakedDist[i + 1] - m_bakedDist[i]);
            }
        }
        return bestOffset;
    }

private:
    static float distanceSquared(const vec2& a, const vec2& b) {
        const vec2 d = a - b;
        return glm::dot(d, d);
    }
    static vec2 closestOnSegment(const vec2& p, const vec2& a, const vec2& b) {
        const vec2 ab = b - a;
        const float len2 = glm::dot(ab, ab);
        if (len2 < 1e-12f) {
            return a;
        }
        const float t = std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
        return a + ab * t;
    }

    std::vector<CurvePoint2D> m_points;
    std::vector<vec2> m_baked;
    std::vector<float> m_bakedDist;
    float m_bakedLength = 0.0f;
};

} // namespace maz::math
