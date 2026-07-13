#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::math {

// 2D computational-geometry helpers — Godot's Geometry2D static class. These are the workhorse
// primitives behind AI line-of-sight, mouse/hit picking, path building, trigger zones, and collision
// pre-checks: does this segment cross that one, what is the nearest point on this edge, is this point
// inside that polygon. Maz had these scattered (triangle area in the triangulator, SAT in ConvexShape2D,
// ray/AABB in Collision); this consolidates the segment/polygon/circle set Godot exposes as one namespace.
// Pure math over vec2 — no allocation, no renderer — so it unit-tests exactly and drives a 2D golden.

struct SegmentHit {
    bool hit = false;
    vec2 point{0.0f, 0.0f}; // intersection point (valid only when hit)
    float t = 0.0f;         // parameter along the first segment a->b, in [0,1]
    float u = 0.0f;         // parameter along the second segment c->d, in [0,1]
};

// Intersection of segment a->b with segment c->d (Godot segment_intersects_segment). Parallel or
// collinear segments report no single-point hit.
inline SegmentHit segmentIntersect(vec2 a, vec2 b, vec2 c, vec2 d) {
    SegmentHit h;
    const vec2 r = b - a;
    const vec2 s = d - c;
    const float rxs = r.x * s.y - r.y * s.x;
    if (std::fabs(rxs) < 1e-9f) {
        return h; // parallel / collinear
    }
    const vec2 ac = c - a;
    const float t = (ac.x * s.y - ac.y * s.x) / rxs;
    const float u = (ac.x * r.y - ac.y * r.x) / rxs;
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        h.hit = true;
        h.t = t;
        h.u = u;
        h.point = a + r * t;
    }
    return h;
}

// The point on segment a->b nearest to p (Godot get_closest_point_to_segment). Clamps to the endpoints.
inline vec2 closestPointOnSegment(vec2 p, vec2 a, vec2 b) {
    const vec2 ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    if (len2 < 1e-12f) {
        return a; // degenerate segment
    }
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return a + ab * t;
}

// Distance from p to segment a->b.
inline float distanceToSegment(vec2 p, vec2 a, vec2 b) {
    const vec2 c = closestPointOnSegment(p, a, b);
    const vec2 d = p - c;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

// Point-in-polygon by even-odd ray casting (Godot is_point_in_polygon). Works for convex OR concave
// simple polygons of either winding. Points exactly on an edge are boundary cases (may report either way).
inline bool pointInPolygon(vec2 p, const std::vector<vec2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const vec2& pi = poly[i];
        const vec2& pj = poly[j];
        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Does segment a->b come within `radius` of `center` (i.e. cross/touch the circle)?
inline bool segmentIntersectsCircle(vec2 a, vec2 b, vec2 center, float radius) {
    return distanceToSegment(center, a, b) <= radius;
}

} // namespace maz::math
