#pragma once

// A uniform Catmull-Rom spline through its control points. evaluate(u) with u in
// [0, segmentCount] interpolates position, passing through every control point
// exactly (endpoints clamped so the curve reaches the first/last point);
// tangent(u) gives the unnormalized travel direction; sample(n) produces a
// polyline for rendering. The Godot Curve3D/Path3D analog for camera/motion paths
// and animation curves; composes iter2-era maz::math (vec3). Parameterization is
// uniform (centripetal/chordal and arc-length reparameterization for constant-speed
// traversal are future refinements). NOT thread-safe for mutation (addPoint); const
// evaluation is pure.

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <vector>
#include <cstddef>
#include <cmath>

namespace maz::math {

class CatmullRomSpline {
public:
    void addPoint(vec3 p) { m_points.push_back(p); }

    std::size_t pointCount() const { return m_points.size(); }

    // A segment connects consecutive control points; N points -> N-1 segments.
    std::size_t segmentCount() const { return m_points.size() >= 2 ? m_points.size() - 1 : 0; }

    const std::vector<vec3>& points() const { return m_points; }

    void clear() { m_points.clear(); }

    // u is a GLOBAL parameter in [0, segmentCount()]: the integer part selects the
    // segment, the fractional part is the local t in [0,1] within it. At every
    // integer u the result is exactly the corresponding control point.
    vec3 evaluate(float u) const {
        MAZ_ASSERT(!m_points.empty(), "CatmullRomSpline::evaluate on empty spline");
        if (m_points.size() == 1) {
            return m_points[0];
        }
        std::size_t seg = 0;
        float t = 0.0f;
        segmentAt(u, seg, t);
        vec3 p0, p1, p2, p3;
        controlPoints(seg, p0, p1, p2, p3);
        // Uniform Catmull-Rom (standard 0.5 tension form).
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                       (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * (t * t) +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t * t));
    }

    // dq/dt: the (unnormalized) direction of travel along the spline.
    vec3 tangent(float u) const {
        MAZ_ASSERT(!m_points.empty(), "CatmullRomSpline::tangent on empty spline");
        if (m_points.size() < 2) {
            return vec3(0.0f); // a single point has no direction
        }
        std::size_t seg = 0;
        float t = 0.0f;
        segmentAt(u, seg, t);
        vec3 p0, p1, p2, p3;
        controlPoints(seg, p0, p1, p2, p3);
        return 0.5f * ((-p0 + p2) +
                       2.0f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t +
                       3.0f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * (t * t));
    }

    // A polyline of points along the whole spline for rendering. Consecutive
    // segments share their boundary point (the boundary u is hit exactly once, so
    // there are no duplicates). front() == first control point, back() == last.
    std::vector<vec3> sample(std::size_t samplesPerSegment) const {
        MAZ_ASSERT(samplesPerSegment >= 1, "CatmullRomSpline::sample needs samplesPerSegment >= 1");
        if (m_points.size() < 2) {
            return m_points; // 0 or 1 points: nothing to interpolate
        }
        const std::size_t totalSteps = segmentCount() * samplesPerSegment;
        std::vector<vec3> out;
        out.reserve(totalSteps + 1);
        const float step = static_cast<float>(samplesPerSegment);
        for (std::size_t i = 0; i <= totalSteps; ++i) {
            out.push_back(evaluate(static_cast<float>(i) / step));
        }
        return out;
    }

private:
    // Decompose a global u into a clamped segment index + local t in [0,1].
    void segmentAt(float u, std::size_t& seg, float& t) const {
        const float maxU = static_cast<float>(segmentCount());
        if (u < 0.0f) {
            u = 0.0f;
        } else if (u > maxU) {
            u = maxU;
        }
        std::size_t s = static_cast<std::size_t>(std::floor(u));
        const std::size_t lastSeg = segmentCount() - 1; // size >= 2 guaranteed by callers
        if (s > lastSeg) {
            s = lastSeg; // u == segmentCount maps to the last segment with t == 1
        }
        seg = s;
        t = u - static_cast<float>(s);
    }

    // Gather the four control points for a segment with endpoint clamping, so the
    // curve passes through the first and last control points.
    void controlPoints(std::size_t seg, vec3& p0, vec3& p1, vec3& p2, vec3& p3) const {
        p1 = m_points[seg];
        p2 = m_points[seg + 1];
        p0 = (seg == 0) ? p1 : m_points[seg - 1];
        p3 = (seg + 2 < m_points.size()) ? m_points[seg + 2] : p2;
    }

    std::vector<vec3> m_points;
};

} // namespace maz::math
