#pragma once

#include "maz/math/Math.hpp"

#include <cstddef>
#include <vector>

namespace maz::math {

// 3D cubic-Bezier path with arc-length baking — Godot's Curve3D, the 3D twin of Curve2D (M142). Each
// point carries a position plus in/out control handles (relative offsets, like Godot); consecutive
// points join by a cubic Bezier. `sample`/`tangent` evaluate the geometric curve; `bake` lays down
// points spaced evenly by ARC LENGTH so `sampleBaked(distance)` moves along the path at CONSTANT speed
// (naive Bezier `t` does not). Backs Path3D / PathFollow3D, camera rails, and 3D spline motion.
// Header-only, deterministic, GPU-free — unit-tests exactly. (Curve3D's per-point tilt/up-vector for
// full PathFollow3D banking is not modelled here; PathFollow3D reports the forward tangent.)

struct CurvePoint3D {
    vec3 position{0.0f, 0.0f, 0.0f};
    vec3 in{0.0f, 0.0f, 0.0f};  // handle leading INTO the point (relative offset)
    vec3 out{0.0f, 0.0f, 0.0f}; // handle leading OUT of the point (relative offset)
};

inline vec3 cubicBezier(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t) {
    const float u = 1.0f - t;
    const float uu = u * u;
    const float tt = t * t;
    return p0 * (uu * u) + p1 * (3.0f * uu * t) + p2 * (3.0f * u * tt) + p3 * (tt * t);
}

class Curve3D {
public:
    void addPoint(const vec3& position, const vec3& in = vec3(0.0f), const vec3& out = vec3(0.0f)) {
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
    const CurvePoint3D& point(std::size_t i) const { return m_points[i]; }

    vec3 sampleSegment(std::size_t seg, float t) const {
        if (m_points.empty()) {
            return vec3(0.0f);
        }
        if (m_points.size() == 1) {
            return m_points[0].position;
        }
        if (seg + 1 >= m_points.size()) {
            seg = m_points.size() - 2;
        }
        const CurvePoint3D& a = m_points[seg];
        const CurvePoint3D& b = m_points[seg + 1];
        return cubicBezier(a.position, a.position + a.out, b.position + b.in, b.position, t);
    }

    vec3 sample(float fofs) const {
        if (m_points.empty()) {
            return vec3(0.0f);
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

    vec3 tangent(float fofs) const {
        const float e = 1e-3f;
        const vec3 d = sample(fofs + e) - sample(fofs - e);
        const float len = glm::length(d);
        return len > 1e-6f ? d / len : vec3(1.0f, 0.0f, 0.0f);
    }

    float length(int stepsPerSegment = 32) const {
        if (m_points.size() < 2 || stepsPerSegment < 1) {
            return 0.0f;
        }
        float total = 0.0f;
        vec3 prev = sampleSegment(0, 0.0f);
        for (std::size_t s = 0; s + 1 < m_points.size(); ++s) {
            for (int i = 1; i <= stepsPerSegment; ++i) {
                const vec3 p = sampleSegment(s, static_cast<float>(i) / static_cast<float>(stepsPerSegment));
                total += glm::length(p - prev);
                prev = p;
            }
        }
        return total;
    }

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

        const int steps = 64;
        std::vector<vec3> dense;
        std::vector<float> denseDist;
        dense.push_back(sampleSegment(0, 0.0f));
        denseDist.push_back(0.0f);
        float acc = 0.0f;
        for (std::size_t s = 0; s + 1 < m_points.size(); ++s) {
            for (int i = 1; i <= steps; ++i) {
                const vec3 p = sampleSegment(s, static_cast<float>(i) / static_cast<float>(steps));
                acc += glm::length(p - dense.back());
                dense.push_back(p);
                denseDist.push_back(acc);
            }
        }
        m_bakedLength = acc;

        std::size_t di = 0;
        for (float d = 0.0f; d <= acc + 1e-4f; d += interval) {
            while (di + 1 < denseDist.size() && denseDist[di + 1] < d) {
                ++di;
            }
            vec3 p;
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
        if (m_bakedDist.empty() || m_bakedDist.back() < acc - 1e-3f) {
            m_baked.push_back(dense.back());
            m_bakedDist.push_back(acc);
        }
    }

    float bakedLength() const { return m_bakedLength; }
    const std::vector<vec3>& bakedPoints() const { return m_baked; }

    vec3 sampleBaked(float distance) {
        if (m_baked.empty()) {
            bake();
        }
        if (m_baked.empty()) {
            return vec3(0.0f);
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

private:
    std::vector<CurvePoint3D> m_points;
    std::vector<vec3> m_baked;
    std::vector<float> m_bakedDist;
    float m_bakedLength = 0.0f;
};

} // namespace maz::math
