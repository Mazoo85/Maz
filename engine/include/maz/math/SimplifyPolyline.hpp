#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math::simplifyPolyline — Ramer-Douglas-Peucker polyline simplification: throw away the points that
// don't matter. Given a chain of points (a hand-drawn stroke, a GPS/replay track, a traced outline, a
// pathfinding result), it returns a shorter chain that stays within a chosen tolerance `epsilon` of the
// original everywhere, keeping only the vertices that carry the shape. It works by keeping the two ends,
// finding the point farthest from the straight line between them, and — if that point is farther than
// epsilon — keeping it and recursing on the two halves; points closer than epsilon to their spanning
// segment are dropped. The result's guarantee: every original point lies within `epsilon` of the simplified
// polyline. This is the standard tool for path/stroke decimation, network/track compression, and reducing
// collision polylines; Godot ships no equivalent (Geometry2D has no line simplifier). The engine already
// has Chaikin *smoothing* (the opposite operation); this is the decimator. Header-only, std-only,
// deterministic; retained points are a subsequence of the input so no new vertices are ever invented.
namespace maz::math {

namespace detail {

// Distance from point p to segment [a,b] (clamped foot), robust to a==b.
inline float pointSegmentDistance(vec2 p, vec2 a, vec2 b) {
    const vec2 ab{b.x - a.x, b.y - a.y};
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    if (len2 > 1e-20f) {
        t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    const vec2 foot{a.x + ab.x * t, a.y + ab.y * t};
    const float dx = p.x - foot.x, dy = p.y - foot.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace detail

// Indices of the retained points (a subsequence of 0..n-1, always including the two endpoints). Iterative
// (explicit stack) so deep chains cannot overflow the call stack.
inline std::vector<std::size_t> simplifyPolylineIndices(const std::vector<vec2>& pts, float epsilon) {
    const std::size_t n = pts.size();
    std::vector<std::size_t> out;
    if (n < 3) {
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back(i);
        }
        return out;
    }
    if (epsilon < 0.0f) {
        epsilon = 0.0f;
    }
    std::vector<unsigned char> keep(n, 0);
    keep[0] = 1;
    keep[n - 1] = 1;
    std::vector<std::pair<std::size_t, std::size_t>> stack;
    stack.push_back({0, n - 1});
    while (!stack.empty()) {
        const std::pair<std::size_t, std::size_t> seg = stack.back();
        stack.pop_back();
        const std::size_t lo = seg.first, hi = seg.second;
        if (hi <= lo + 1) {
            continue; // nothing strictly between the two ends
        }
        float best = -1.0f;
        std::size_t bestIdx = lo;
        for (std::size_t i = lo + 1; i < hi; ++i) {
            const float d = detail::pointSegmentDistance(pts[i], pts[lo], pts[hi]);
            if (d > best) {
                best = d;
                bestIdx = i;
            }
        }
        if (best > epsilon) {
            keep[bestIdx] = 1;
            stack.push_back({lo, bestIdx});
            stack.push_back({bestIdx, hi});
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (keep[i]) {
            out.push_back(i);
        }
    }
    return out;
}

// The simplified polyline itself (retained points, in order).
inline std::vector<vec2> simplifyPolyline(const std::vector<vec2>& pts, float epsilon) {
    const std::vector<std::size_t> idx = simplifyPolylineIndices(pts, epsilon);
    std::vector<vec2> out;
    out.reserve(idx.size());
    for (std::size_t i : idx) {
        out.push_back(pts[i]);
    }
    return out;
}

} // namespace maz::math
