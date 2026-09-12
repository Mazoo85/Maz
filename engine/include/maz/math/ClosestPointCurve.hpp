#pragma once

#include "maz/math/Curve2D.hpp" // Curve2D, vec2
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math closest-point / projection onto a path — given any point in space, find the nearest point on a
// polyline or on a smooth Bezier curve, plus HOW FAR ALONG the path that nearest point sits. This is the
// query behind snapping a dragged object to a spline, measuring an agent's progress along a race line or
// rail, keeping a follower glued to a track, computing a car's cross-track error, or finding the distance
// from anything to a route. Godot's Curve2D exposes sampling and baking but no "project this point onto the
// curve" call, so gameplay code has to roll it by hand. This does it robustly: exact per-segment projection
// for polylines, and a dense-sample-plus-local-refine search for curves. Header-only, std-only,
// deterministic.
namespace maz::math {

struct SegmentProjection {
    vec2 point{0.0f, 0.0f};
    float t = 0.0f; // parameter along the segment, clamped to [0,1]
};

// Nearest point on segment a->b to p (t clamped to the segment).
inline SegmentProjection closestPointOnSegment(const vec2& a, const vec2& b, const vec2& p) {
    const vec2 ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (len2 > 1e-20f) {
        t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    return SegmentProjection{vec2(a.x + ab.x * t, a.y + ab.y * t), t};
}

struct CurveProjection {
    vec2 point{0.0f, 0.0f};
    float offset = 0.0f;   // position along the path: segment index + t for polylines, curve offset for curves
    float distance = 0.0f; // Euclidean distance from the query point to `point`
    std::size_t segment = 0;
};

// Nearest point on a polyline (open) to p. `offset` is segment + t. Empty input yields a zeroed result.
inline CurveProjection closestPointOnPolyline(const std::vector<vec2>& pts, const vec2& p) {
    CurveProjection best;
    if (pts.empty()) {
        return best;
    }
    if (pts.size() == 1) {
        best.point = pts[0];
        best.distance = glm::length(p - pts[0]);
        return best;
    }
    float bestD2 = 1e30f;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const SegmentProjection sp = closestPointOnSegment(pts[i], pts[i + 1], p);
        const float dx = p.x - sp.point.x, dy = p.y - sp.point.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestD2) {
            bestD2 = d2;
            best.point = sp.point;
            best.segment = i;
            best.offset = static_cast<float>(i) + sp.t;
            best.distance = std::sqrt(d2);
        }
    }
    return best;
}

// Nearest point on a Bezier Curve2D to p. Densely samples the curve into a polyline (samplesPerSegment per
// segment), finds the closest sample interval, then refines with a few ternary-search iterations on the
// squared distance for sub-sample accuracy. `offset` is the curve's fractional point offset (as consumed by
// Curve2D::sample), so callers can resample tangents/positions there.
inline CurveProjection closestPointOnCurve(const Curve2D& curve, const vec2& p, int samplesPerSegment = 16) {
    CurveProjection best;
    const std::size_t n = curve.pointCount();
    if (n == 0) {
        return best;
    }
    if (n == 1) {
        best.point = curve.sample(0.0f);
        best.distance = glm::length(p - best.point);
        return best;
    }
    const float maxOfs = static_cast<float>(n - 1);
    const int steps = samplesPerSegment < 1 ? 1 : samplesPerSegment;
    const float df = 1.0f / static_cast<float>(steps);
    // Coarse scan.
    float bestOfs = 0.0f, bestD2 = 1e30f;
    for (float f = 0.0f; f <= maxOfs + 1e-6f; f += df) {
        const float ff = f > maxOfs ? maxOfs : f;
        const vec2 s = curve.sample(ff);
        const float dx = p.x - s.x, dy = p.y - s.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestD2) {
            bestD2 = d2;
            bestOfs = ff;
        }
    }
    // Ternary refine within +/- one coarse step.
    float lo = bestOfs - df, hi = bestOfs + df;
    lo = lo < 0.0f ? 0.0f : lo;
    hi = hi > maxOfs ? maxOfs : hi;
    auto d2at = [&](float f) {
        const vec2 s = curve.sample(f);
        const float dx = p.x - s.x, dy = p.y - s.y;
        return dx * dx + dy * dy;
    };
    for (int it = 0; it < 40 && hi - lo > 1e-6f; ++it) {
        const float m1 = lo + (hi - lo) / 3.0f;
        const float m2 = hi - (hi - lo) / 3.0f;
        if (d2at(m1) < d2at(m2)) {
            hi = m2;
        } else {
            lo = m1;
        }
    }
    const float ofs = 0.5f * (lo + hi);
    best.point = curve.sample(ofs);
    best.offset = ofs;
    best.segment = static_cast<std::size_t>(std::floor(ofs));
    best.distance = std::sqrt(d2at(ofs));
    return best;
}

} // namespace maz::math
