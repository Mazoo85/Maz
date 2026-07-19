#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
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

// Is `p` inside (or exactly on) the circle at `center` with `radius`? — Godot's Geometry2D
// is_point_in_circle. Compares squared distance to radius² (boundary counts as inside).
inline bool pointInCircle(vec2 p, vec2 center, float radius) {
    const vec2 d = p - center;
    return dot(d, d) <= radius * radius;
}

// ---- more Geometry2D statics (M276) ---------------------------------------------------------

// Closest point on the INFINITE line through a,b (unclamped) — Godot's
// Geometry2D.get_closest_point_to_segment_uncapped.
inline vec2 closestPointOnLine(vec2 p, vec2 a, vec2 b) {
    const vec2 ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    if (len2 < 1e-12f) {
        return a;
    }
    const float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
    return a + ab * t;
}

// Intersection of two INFINITE lines given a point + direction each; nullopt if parallel — Godot's
// Geometry2D.line_intersects_line.
inline std::optional<vec2> lineIntersectsLine(vec2 fromA, vec2 dirA, vec2 fromB, vec2 dirB) {
    const float denom = dirA.x * dirB.y - dirA.y * dirB.x;
    if (std::fabs(denom) < 1e-12f) {
        return std::nullopt; // parallel or coincident
    }
    const vec2 d = fromB - fromA;
    const float t = (d.x * dirB.y - d.y * dirB.x) / denom;
    return fromA + dirA * t;
}

// Barycentric point-in-triangle test (inclusive of edges) — Godot's Geometry2D.point_is_inside_triangle.
inline bool pointInTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    const float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    const float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    const float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos); // all same sign (allowing zeros on edges)
}

// Closest points between two segments [p1,q1] and [p2,q2] (Ericson's clamped solver) — Godot's
// Geometry2D.get_closest_points_between_segments. Writes the pair into c1/c2.
inline void closestPointsBetweenSegments(vec2 p1, vec2 q1, vec2 p2, vec2 q2, vec2& c1, vec2& c2) {
    const vec2 d1 = q1 - p1; // direction of segment 1
    const vec2 d2 = q2 - p2; // direction of segment 2
    const vec2 r = p1 - p2;
    const float a = d1.x * d1.x + d1.y * d1.y;
    const float e = d2.x * d2.x + d2.y * d2.y;
    const float f = d2.x * r.x + d2.y * r.y;
    float s, t;
    if (a < 1e-12f && e < 1e-12f) {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a < 1e-12f) {
        s = 0.0f;
        t = std::min(std::max(f / e, 0.0f), 1.0f);
    } else {
        const float c = d1.x * r.x + d1.y * r.y;
        if (e < 1e-12f) {
            t = 0.0f;
            s = std::min(std::max(-c / a, 0.0f), 1.0f);
        } else {
            const float b = d1.x * d2.x + d1.y * d2.y;
            const float denom = a * e - b * b;
            s = denom > 1e-12f ? std::min(std::max((b * f - c * e) / denom, 0.0f), 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::min(std::max(-c / a, 0.0f), 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::min(std::max((b - c) / a, 0.0f), 1.0f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

// ---- polygon toolkit (Godot's Geometry2D polygon helpers) -----------------------------------

// Signed area of a polygon (shoelace). Positive when the vertices wind counter-clockwise in a
// conventional Y-up frame; the magnitude is the enclosed area. Fewer than 3 points -> 0.
inline float polygonArea(const std::vector<vec2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& a = poly[i];
        const vec2& b = poly[(i + 1) % n];
        sum += a.x * b.y - b.x * a.y;
    }
    return sum * 0.5f;
}

// True when the polygon winds clockwise in Godot's SCREEN space (Y points down) — matches Godot's
// Geometry2D.is_polygon_clockwise (sum of (x2-x1)(y2+y1) > 0). Note this is the opposite sign from
// polygonArea, which uses the Y-up math convention.
inline bool isPolygonClockwise(const std::vector<vec2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) {
        return false;
    }
    float sum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& a = poly[i];
        const vec2& b = poly[(i + 1) % n];
        sum += (b.x - a.x) * (b.y + a.y);
    }
    return sum > 0.0f;
}

// Area-weighted centroid (centre of mass) of a simple polygon. Falls back to the vertex average when
// the polygon is degenerate (near-zero area).
inline vec2 polygonCentroid(const std::vector<vec2>& poly) {
    const std::size_t n = poly.size();
    if (n == 0) {
        return vec2(0.0f, 0.0f);
    }
    float a2 = 0.0f;   // twice the signed area
    vec2 c(0.0f, 0.0f);
    for (std::size_t i = 0; i < n; ++i) {
        const vec2& p = poly[i];
        const vec2& q = poly[(i + 1) % n];
        const float cross = p.x * q.y - q.x * p.y;
        a2 += cross;
        c.x += (p.x + q.x) * cross;
        c.y += (p.y + q.y) * cross;
    }
    if (std::fabs(a2) < 1e-8f) {
        vec2 avg(0.0f, 0.0f);
        for (const vec2& p : poly) {
            avg += p;
        }
        return avg / static_cast<float>(n);
    }
    return c / (3.0f * a2);
}

// Convex hull of a point set via Andrew's monotone chain, O(n log n). Returns the hull vertices in
// counter-clockwise order (Y-up), no duplicated closing point. Fewer than 3 unique points return the
// input's extremes as-is. Godot's Geometry2D.convex_hull.
inline std::vector<vec2> convexHull(std::vector<vec2> pts) {
    const std::size_t n = pts.size();
    if (n < 3) {
        return pts;
    }
    std::sort(pts.begin(), pts.end(), [](const vec2& a, const vec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    // 2D cross of OA x OB; > 0 => counter-clockwise turn.
    auto cross = [](const vec2& o, const vec2& a, const vec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<vec2> hull(2 * n);
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) { // lower hull
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
            --k;
        }
        hull[k++] = pts[i];
    }
    const std::size_t lower = k + 1;
    for (std::size_t i = n - 1; i-- > 0;) { // upper hull
        while (k >= lower && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
            --k;
        }
        hull[k++] = pts[i];
    }
    hull.resize(k - 1); // drop the repeated start point
    return hull;
}

// Clip a subject polygon against a CONVEX clip polygon (Sutherland–Hodgman) — the convex case of
// Godot's Geometry2D.clip_polygons/intersect_polygons: keeps the part of `subject` that lies inside
// `convexClip`. Both must be counter-clockwise (Y-up); the clip polygon must be convex (a viewport
// rect, an FOV wedge, a scissor region). Returns the clipped polygon, empty when fully outside.
// General (non-convex, multi-contour) boolean polygon ops via Clipper remain out of scope. (M296)
inline std::vector<vec2> clipPolygonConvex(const std::vector<vec2>& subject,
                                           const std::vector<vec2>& convexClip) {
    if (subject.size() < 3 || convexClip.size() < 3) {
        return {};
    }
    std::vector<vec2> output = subject;
    const std::size_t m = convexClip.size();
    for (std::size_t e = 0; e < m; ++e) {
        const vec2 a = convexClip[e];
        const vec2 b = convexClip[(e + 1) % m];
        // Inside = left of the directed clip edge a->b (CCW convex): cross(b-a, p-a) >= 0.
        auto inside = [&](const vec2& p) {
            return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x) >= 0.0f;
        };
        auto intersect = [&](const vec2& p, const vec2& q) {
            const vec2 d1 = b - a;
            const vec2 d2 = q - p;
            const float denom = d1.x * d2.y - d1.y * d2.x;
            if (std::abs(denom) < 1e-12f) {
                return q; // parallel; degenerate, keep endpoint
            }
            const float t = ((p.x - a.x) * d1.y - (p.y - a.y) * d1.x) / denom;
            return vec2(p.x + t * d2.x, p.y + t * d2.y);
        };

        const std::vector<vec2> input = output;
        output.clear();
        if (input.empty()) {
            break;
        }
        for (std::size_t i = 0; i < input.size(); ++i) {
            const vec2 cur = input[i];
            const vec2 prev = input[(i + input.size() - 1) % input.size()];
            const bool curIn = inside(cur);
            const bool prevIn = inside(prev);
            if (curIn) {
                if (!prevIn) {
                    output.push_back(intersect(prev, cur));
                }
                output.push_back(cur);
            } else if (prevIn) {
                output.push_back(intersect(prev, cur));
            }
        }
    }
    return output;
}

// Clip a segment [a,b] to the axis-aligned rectangle [rmin, rmax] (Liang–Barsky). Returns the clipped
// sub-segment, or nullopt when the segment lies entirely outside the rect. The portion inside the
// rectangle is what's kept — the standard viewport/bounds clip for lines, laser sights and debug rays.
// (M303, toward Godot's Geometry2D/Rect2 segment clipping.)
inline std::optional<std::pair<vec2, vec2>> clipSegmentToRect(const vec2& a, const vec2& b,
                                                             const vec2& rmin, const vec2& rmax) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    float t0 = 0.0f, t1 = 1.0f;
    // Each edge: p*t <= q. Accept-only test that shrinks [t0,t1].
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {a.x - rmin.x, rmax.x - a.x, a.y - rmin.y, rmax.y - a.y};
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(p[i]) < 1e-12f) {
            if (q[i] < 0.0f) {
                return std::nullopt; // parallel and outside this boundary
            }
            continue;
        }
        const float r = q[i] / p[i];
        if (p[i] < 0.0f) {
            if (r > t1) {
                return std::nullopt;
            }
            if (r > t0) {
                t0 = r;
            }
        } else {
            if (r < t0) {
                return std::nullopt;
            }
            if (r < t1) {
                t1 = r;
            }
        }
    }
    const vec2 c0(a.x + t0 * dx, a.y + t0 * dy);
    const vec2 c1(a.x + t1 * dx, a.y + t1 * dy);
    return std::make_pair(c0, c1);
}

} // namespace maz::math
