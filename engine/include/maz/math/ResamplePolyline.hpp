#pragma once

#include "maz/math/Math.hpp" // vec2, vec3, dot

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math polyline resampling — redistribute the points of a discrete polyline so they are EVENLY SPACED by
// arc length. A hand-drawn stroke, a GPS/replay track, a traced outline, or a spline flattened to points has
// clumped, unevenly-spaced vertices; this walks the path and drops new points at equal distances so you can
// place fence posts / trees / footprints along a route, draw a dashed or dotted line, emit uniform trail
// segments, or space patrol waypoints. Two forms: `resamplePolyline` gives exactly N points (endpoints
// preserved), `resamplePolylineBySpacing` gives a point every `spacing` units from the start. Distinct from
// the engine's other polyline tools — SimplifyPolyline THROWS AWAY points (Douglas–Peucker), PolylineStroke
// THICKENS a path into a fillable outline, and ArcLength reparameterizes a PARAMETRIC curve; this evenly
// re-spaces an existing point list. Works on vec2 or vec3. Header-only, deterministic.
namespace maz::math {

namespace detail {
template <class V>
inline float polyLen(const V& d) {
    return std::sqrt(dot(d, d));
}
} // namespace detail

// N points evenly spaced by arc length along the polyline `pts`; the first and last points are preserved
// exactly. `count < 1` returns empty; `count == 1` returns the first point; a zero-length (all-coincident)
// path returns `count` copies of the first point.
template <class V>
inline std::vector<V> resamplePolyline(const std::vector<V>& pts, int count) {
    std::vector<V> out;
    if (pts.empty() || count < 1) {
        return out;
    }
    if (count == 1 || pts.size() == 1) {
        out.assign(static_cast<std::size_t>(count), pts.front());
        return out;
    }
    // Cumulative arc length at each vertex.
    std::vector<float> cum(pts.size(), 0.0f);
    for (std::size_t i = 1; i < pts.size(); ++i) {
        cum[i] = cum[i - 1] + detail::polyLen(pts[i] - pts[i - 1]);
    }
    const float total = cum.back();
    if (total <= 1e-12f) {
        out.assign(static_cast<std::size_t>(count), pts.front());
        return out;
    }

    out.reserve(static_cast<std::size_t>(count));
    out.push_back(pts.front());
    std::size_t seg = 1; // index of the segment end we're scanning toward
    for (int i = 1; i < count - 1; ++i) {
        const float target = total * static_cast<float>(i) / static_cast<float>(count - 1);
        while (seg < pts.size() - 1 && cum[seg] < target) {
            ++seg;
        }
        const float segLen = cum[seg] - cum[seg - 1];
        const float t = segLen > 1e-12f ? (target - cum[seg - 1]) / segLen : 0.0f;
        out.push_back(pts[seg - 1] + (pts[seg] - pts[seg - 1]) * t);
    }
    out.push_back(pts.back());
    return out;
}

// A point every `spacing` units of arc length, starting at the first point: at distances 0, spacing, 2·spacing,
// … up to the total length (the exact final endpoint is included only if it lands on a multiple of `spacing`).
// `spacing <= 0` or an empty path returns empty; a single point / zero-length path returns just the first point.
template <class V>
inline std::vector<V> resamplePolylineBySpacing(const std::vector<V>& pts, float spacing) {
    std::vector<V> out;
    if (pts.empty() || spacing <= 0.0f) {
        return out;
    }
    out.push_back(pts.front());
    if (pts.size() == 1) {
        return out;
    }
    float target = spacing;
    float travelled = 0.0f; // total arc length walked so far
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const V a = pts[i - 1];
        const V b = pts[i];
        const float segLen = detail::polyLen(b - a);
        while (segLen > 1e-12f && target <= travelled + segLen + 1e-6f) {
            const float t = (target - travelled) / segLen;
            const float tc = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            out.push_back(a + (b - a) * tc);
            target += spacing;
        }
        travelled += segLen;
    }
    return out;
}

} // namespace maz::math
