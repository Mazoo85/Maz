#pragma once

#include "maz/math/Math.hpp"
#include "maz/math/VectorOps.hpp" // cubicInterpolate (Catmull-Rom segment) for catmullRomSpline

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

// Decompose a simple polygon (convex or concave, no holes) into convex polygons whose union is the
// original — Godot's Geometry2D.decompose_polygon_in_convex. Ear-clips into triangles, then greedily
// merges edge-adjacent pieces (Hertel–Mehlhorn) while the union stays convex. Guarantees every
// returned polygon is convex, their areas sum to the input's, and a convex input collapses to a
// single piece. Godot only promises A valid convex cover (not a specific partition); this returns
// one, in counter-clockwise (Y-up) winding. Fewer than 3 vertices -> empty.
inline std::vector<std::vector<vec2>> decomposePolygonInConvex(const std::vector<vec2>& polyIn) {
    if (polyIn.size() < 3) {
        return {};
    }
    // Normalise to counter-clockwise (positive shoelace area) so cross > 0 means a left turn.
    std::vector<vec2> poly = polyIn;
    {
        float area2 = 0.0f;
        const std::size_t n = poly.size();
        for (std::size_t i = 0; i < n; ++i) {
            const vec2& p = poly[i];
            const vec2& q = poly[(i + 1) % n];
            area2 += p.x * q.y - q.x * p.y;
        }
        if (area2 < 0.0f) {
            std::reverse(poly.begin(), poly.end());
        }
    }
    auto cross3 = [](const vec2& a, const vec2& b, const vec2& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };
    auto pointInTri = [&](const vec2& p, const vec2& a, const vec2& b, const vec2& c) {
        const float d1 = cross3(a, b, p), d2 = cross3(b, c, p), d3 = cross3(c, a, p);
        const bool hasNeg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
        const bool hasPos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
        return !(hasNeg && hasPos); // inside or on an edge
    };
    // --- ear clipping into CCW triangles ---
    std::vector<int> idx(poly.size());
    for (std::size_t i = 0; i < poly.size(); ++i) {
        idx[i] = static_cast<int>(i);
    }
    std::vector<std::vector<vec2>> pieces;
    int guard = 0;
    while (idx.size() > 3 && guard++ < 100000) {
        bool clipped = false;
        const int n = static_cast<int>(idx.size());
        for (int i = 0; i < n; ++i) {
            const vec2& a = poly[static_cast<std::size_t>(idx[static_cast<std::size_t>((i + n - 1) % n)])];
            const vec2& b = poly[static_cast<std::size_t>(idx[static_cast<std::size_t>(i)])];
            const vec2& c = poly[static_cast<std::size_t>(idx[static_cast<std::size_t>((i + 1) % n)])];
            if (cross3(a, b, c) <= 0.0f) {
                continue; // reflex or degenerate corner: not an ear
            }
            bool ear = true;
            for (int j = 0; j < n; ++j) {
                if (j == (i + n - 1) % n || j == i || j == (i + 1) % n) {
                    continue;
                }
                if (pointInTri(poly[static_cast<std::size_t>(idx[static_cast<std::size_t>(j)])], a, b, c)) {
                    ear = false;
                    break;
                }
            }
            if (ear) {
                pieces.push_back({a, b, c});
                idx.erase(idx.begin() + i);
                clipped = true;
                break;
            }
        }
        if (!clipped) {
            break; // degenerate polygon: stop with what we have
        }
    }
    if (idx.size() == 3) {
        pieces.push_back({poly[static_cast<std::size_t>(idx[0])], poly[static_cast<std::size_t>(idx[1])],
                          poly[static_cast<std::size_t>(idx[2])]});
    }
    // --- Hertel–Mehlhorn: merge edge-adjacent pieces while the result stays convex ---
    auto isConvexCCW = [&](const std::vector<vec2>& p) {
        const int n = static_cast<int>(p.size());
        if (n < 3) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            if (cross3(p[static_cast<std::size_t>(i)], p[static_cast<std::size_t>((i + 1) % n)],
                       p[static_cast<std::size_t>((i + 2) % n)]) < -1e-4f) {
                return false;
            }
        }
        return true;
    };
    auto same = [](const vec2& u, const vec2& v) {
        return std::fabs(u.x - v.x) < 1e-5f && std::fabs(u.y - v.y) < 1e-5f;
    };
    bool merged = true;
    guard = 0;
    while (merged && guard++ < 100000) {
        merged = false;
        for (std::size_t pi = 0; pi < pieces.size() && !merged; ++pi) {
            for (std::size_t qi = pi + 1; qi < pieces.size() && !merged; ++qi) {
                const std::vector<vec2>& P = pieces[pi];
                const std::vector<vec2>& Q = pieces[qi];
                const int np = static_cast<int>(P.size());
                const int nq = static_cast<int>(Q.size());
                for (int i = 0; i < np && !merged; ++i) {
                    const vec2& a = P[static_cast<std::size_t>(i)];
                    const vec2& b = P[static_cast<std::size_t>((i + 1) % np)];
                    for (int j = 0; j < nq; ++j) {
                        // Shared edge: P's a->b is the reverse of Q's edge (Q[j]=b, Q[j+1]=a).
                        if (same(Q[static_cast<std::size_t>(j)], b) &&
                            same(Q[static_cast<std::size_t>((j + 1) % nq)], a)) {
                            std::vector<vec2> m;
                            m.reserve(static_cast<std::size_t>(np + nq - 2));
                            for (int k = 0; k < np; ++k) {
                                m.push_back(P[static_cast<std::size_t>((i + 1 + k) % np)]); // b .. a
                            }
                            for (int k = 0; k < nq - 2; ++k) {
                                m.push_back(Q[static_cast<std::size_t>((j + 2 + k) % nq)]); // Q interior
                            }
                            if (isConvexCCW(m)) {
                                pieces[pi] = m;
                                pieces.erase(pieces.begin() + static_cast<std::ptrdiff_t>(qi));
                                merged = true;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return pieces;
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

// Offset (inflate for delta > 0, deflate for delta < 0) a CONVEX polygon by `delta` with MITRE joins —
// the convex case of Godot's Geometry2D.offset_polygon. Each edge is pushed out along its outward
// normal and consecutive offset edges are re-intersected at the corners; the result is returned
// counter-clockwise. Use for collision margins, selection outlines and grow/shrink of convex hulls.
// General (concave, multi-contour, round/square joins) Clipper offsetting remains out of scope; a
// large negative delta that collapses the polygon yields a degenerate result. < 3 vertices -> input.
inline std::vector<vec2> offsetPolygonConvex(const std::vector<vec2>& polyIn, float delta) {
    const std::size_t n = polyIn.size();
    if (n < 3) {
        return polyIn;
    }
    // Normalise to counter-clockwise so the outward normal (edgeDir rotated -90°) points away.
    std::vector<vec2> poly = polyIn;
    {
        float area2 = 0.0f;
        for (std::size_t i = 0; i < n; ++i) {
            const vec2& p = poly[i];
            const vec2& q = poly[(i + 1) % n];
            area2 += p.x * q.y - q.x * p.y;
        }
        if (area2 < 0.0f) {
            std::reverse(poly.begin(), poly.end());
        }
    }
    // Per-edge outward unit normal: for CCW winding the outward direction of edge e is (e.y, -e.x).
    std::vector<vec2> normal(n);
    for (std::size_t i = 0; i < n; ++i) {
        const vec2 e = poly[(i + 1) % n] - poly[i];
        const vec2 nrm(e.y, -e.x);
        const float len = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y);
        normal[i] = len > 1e-12f ? vec2(nrm.x / len, nrm.y / len) : vec2(0.0f, 0.0f);
    }
    std::vector<vec2> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t prev = (i + n - 1) % n; // edge prev->i
        const vec2 aFrom = poly[prev] + normal[prev] * delta;
        const vec2 aDir = poly[i] - poly[prev];
        const vec2 bFrom = poly[i] + normal[i] * delta;
        const vec2 bDir = poly[(i + 1) % n] - poly[i];
        const std::optional<vec2> hit = lineIntersectsLine(aFrom, aDir, bFrom, bDir);
        out[i] = hit ? *hit : (poly[i] + normal[i] * delta); // parallel (straight vertex): just shift
    }
    return out;
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

// A 2D circle (centre + radius), the result type of minEnclosingCircle.
struct Circle2 {
    vec2 center{0.0f, 0.0f};
    float radius = 0.0f;
    bool contains(vec2 p, float eps = 1e-4f) const {
        const float dx = p.x - center.x, dy = p.y - center.y;
        return dx * dx + dy * dy <= (radius + eps) * (radius + eps);
    }
};

namespace detail {

inline Circle2 circleFromTwo(vec2 a, vec2 b) {
    const vec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    const float dx = a.x - c.x, dy = a.y - c.y;
    return Circle2{c, std::sqrt(dx * dx + dy * dy)};
}

// Circumcircle of three points; radius < 0 signals a degenerate (collinear) triple.
inline Circle2 circleFromThree(vec2 a, vec2 b, vec2 c) {
    const float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (std::fabs(d) < 1e-12f) {
        return Circle2{vec2(0.0f, 0.0f), -1.0f};
    }
    const float a2 = a.x * a.x + a.y * a.y;
    const float b2 = b.x * b.x + b.y * b.y;
    const float c2 = c.x * c.x + c.y * c.y;
    const float ux = (a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d;
    const float uy = (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d;
    const vec2 ctr(ux, uy);
    const float rx = a.x - ux, ry = a.y - uy;
    return Circle2{ctr, std::sqrt(rx * rx + ry * ry)};
}

} // namespace detail

// Smallest circle enclosing all `points` — Welzl's minimal-enclosing-circle via Nayuki's
// deterministic incremental algorithm. Every point ends up inside (or on) the result and the circle
// is the unique smallest such; empty input yields a zero circle, one point a zero-radius circle.
// Handy for bounding-volume culling, broadphase bounds, and "fit the view to these points". O(n)
// expected on shuffled input; deterministic here (fixed order), so worst case O(n^2) — fine for the
// modest point counts these bounds are built from.
inline Circle2 minEnclosingCircle(const std::vector<vec2>& points) {
    const std::size_t n = points.size();
    if (n == 0) {
        return Circle2{vec2(0.0f, 0.0f), 0.0f};
    }
    Circle2 c{points[0], 0.0f};
    bool has = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (has && c.contains(points[i])) {
            continue;
        }
        // points[i] must lie on the boundary of the circle of points[0..i].
        c = Circle2{points[i], 0.0f};
        for (std::size_t j = 0; j < i; ++j) {
            if (c.contains(points[j])) {
                continue;
            }
            if (c.radius == 0.0f) {
                c = detail::circleFromTwo(points[i], points[j]);
            } else {
                // points[i] and points[j] on the boundary; find the third.
                Circle2 cc{detail::circleFromTwo(points[i], points[j])};
                for (std::size_t k = 0; k < j; ++k) {
                    if (cc.contains(points[k])) {
                        continue;
                    }
                    const Circle2 tri = detail::circleFromThree(points[i], points[j], points[k]);
                    if (tri.radius >= 0.0f) {
                        cc = tri;
                    }
                }
                c = cc;
            }
        }
        has = true;
    }
    return c;
}

// Ramer-Douglas-Peucker polyline simplification: drop points that lie within `tolerance` of the line
// through the segment's endpoints, keeping the overall shape while shedding redundant vertices. This is
// the workhorse behind cleaning up hand-drawn strokes and gestures, thinning GPS/AI paths, and reducing
// vertex counts on generated outlines. Endpoints are always kept; a collinear run collapses to its two
// ends; a corner farther than `tolerance` from the chord survives. Non-recursive (explicit range stack)
// so it is safe on very long inputs. tolerance < 0 is treated as 0. Godot has no polyline simplify.
inline std::vector<vec2> simplifyPolyline(const std::vector<vec2>& pts, float tolerance) {
    const std::size_t n = pts.size();
    if (n <= 2) {
        return pts;
    }
    const float tol = tolerance > 0.0f ? tolerance : 0.0f;

    // Perpendicular distance from p to the line through a and b (point distance if a == b).
    auto perpDist = [](vec2 p, vec2 a, vec2 b) -> float {
        const vec2 ab = b - a;
        const float len2 = ab.x * ab.x + ab.y * ab.y;
        const vec2 ap = p - a;
        if (len2 <= 0.0f) {
            return std::sqrt(ap.x * ap.x + ap.y * ap.y);
        }
        const float cross = ab.x * ap.y - ab.y * ap.x;
        return std::fabs(cross) / std::sqrt(len2);
    };

    std::vector<bool> keep(n, false);
    keep[0] = true;
    keep[n - 1] = true;

    std::vector<std::pair<std::size_t, std::size_t>> stack;
    stack.emplace_back(0, n - 1);
    while (!stack.empty()) {
        const std::pair<std::size_t, std::size_t> range = stack.back();
        stack.pop_back();
        const std::size_t lo = range.first;
        const std::size_t hi = range.second;
        if (hi <= lo + 1) {
            continue; // nothing between the endpoints
        }
        float maxDist = -1.0f;
        std::size_t maxIdx = lo;
        for (std::size_t i = lo + 1; i < hi; ++i) {
            const float d = perpDist(pts[i], pts[lo], pts[hi]);
            if (d > maxDist) {
                maxDist = d;
                maxIdx = i;
            }
        }
        if (maxDist > tol) {
            keep[maxIdx] = true;
            stack.emplace_back(lo, maxIdx);
            stack.emplace_back(maxIdx, hi);
        }
    }

    std::vector<vec2> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (keep[i]) {
            out.push_back(pts[i]);
        }
    }
    return out;
}

// Chaikin corner-cutting: round off a coarse polyline into a smooth one by replacing each interior
// corner with two points 1/4 and 3/4 of the way along its two edges, repeated `iterations` times. This
// is the workhorse behind smoothing AI/nav paths, hand-drawn strokes and generated outlines into gentle
// curves without a spline fit — the complement of simplifyPolyline (which thins; this rounds). For an
// open path the two endpoints are preserved; set `closed` for a loop (every vertex is cut, no endpoints).
// Each iteration roughly doubles the point count; iterations <= 0 or < 3 points returns the input as-is.
inline std::vector<vec2> chaikinSmooth(const std::vector<vec2>& pts, int iterations, bool closed = false) {
    if (iterations <= 0 || pts.size() < 3) {
        return pts;
    }
    std::vector<vec2> cur = pts;
    for (int it = 0; it < iterations; ++it) {
        const std::size_t n = cur.size();
        if (n < 3) {
            break;
        }
        std::vector<vec2> next;
        next.reserve(closed ? n * 2 : (n * 2));
        if (!closed) {
            next.push_back(cur.front()); // keep the first endpoint
        }
        const std::size_t last = closed ? n : (n - 1);
        for (std::size_t i = 0; i < last; ++i) {
            const vec2 a = cur[i];
            const vec2 b = cur[(i + 1) % n];
            // Q = 3/4 a + 1/4 b, R = 1/4 a + 3/4 b.
            next.push_back(vec2(0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y));
            next.push_back(vec2(0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y));
        }
        if (!closed) {
            next.push_back(cur.back()); // keep the last endpoint
        }
        cur = std::move(next);
    }
    return cur;
}

// Total arc length of a polyline (sum of edge lengths); set `closed` to include the wrap edge back to
// the start. The natural companion to resamplePolyline and handy for path budgets / travel times.
inline float polylineLength(const std::vector<vec2>& pts, bool closed = false) {
    const std::size_t n = pts.size();
    if (n < 2) {
        return 0.0f;
    }
    float total = 0.0f;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const vec2 d = pts[i + 1] - pts[i];
        total += std::sqrt(d.x * d.x + d.y * d.y);
    }
    if (closed) {
        const vec2 d = pts[0] - pts[n - 1];
        total += std::sqrt(d.x * d.x + d.y * d.y);
    }
    return total;
}

// Resample a polyline to exactly `count` points spread evenly by ARC LENGTH along the path, keeping the
// original endpoints. This is the workhorse behind evenly spacing dashes / decorations / spawn points
// along a route, and uniform sampling for morphing or per-point animation. Unlike Curve2D's Bézier
// baking, this works on any raw polyline. count < 2 or < 2 input points returns the input unchanged; an
// all-coincident path returns `count` copies of the first point (no divide-by-zero).
inline std::vector<vec2> resamplePolyline(const std::vector<vec2>& pts, int count) {
    const std::size_t n = pts.size();
    if (count < 2 || n < 2) {
        return pts;
    }
    const float total = polylineLength(pts);
    if (total <= 0.0f) {
        return std::vector<vec2>(static_cast<std::size_t>(count), pts.front());
    }
    const float step = total / static_cast<float>(count - 1);

    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(count));
    out.push_back(pts.front());

    std::size_t seg = 0;
    float segStart = 0.0f; // arc length at the start of the current segment
    auto edgeLen = [&](std::size_t i) {
        const vec2 d = pts[i + 1] - pts[i];
        return std::sqrt(d.x * d.x + d.y * d.y);
    };
    float segLen = edgeLen(0);
    for (int k = 1; k < count - 1; ++k) {
        const float target = static_cast<float>(k) * step;
        while (seg + 1 < n - 1 && segStart + segLen < target) {
            segStart += segLen;
            ++seg;
            segLen = edgeLen(seg);
        }
        const float t = segLen > 0.0f ? (target - segStart) / segLen : 0.0f;
        const vec2 a = pts[seg];
        const vec2 b = pts[seg + 1];
        out.push_back(vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
    }
    out.push_back(pts.back());
    return out;
}

// Catmull-Rom spline through a list of waypoints: a smooth curve that PASSES THROUGH every point (unlike
// a Bezier, whose control points only pull the curve). This is what you want for a camera or object path
// that must hit exact waypoints, or for rounding a coarse route into a flowing curve. Each segment is
// evaluated with the existing cubicInterpolate (Catmull-Rom) using the two neighbouring points as
// tangents; boundaries clamp the missing neighbour (open) or wrap (closed). Returns `samplesPerSegment`
// points per segment plus the final endpoint for an open path (a closed loop returns exactly
// N*samplesPerSegment points). samplesPerSegment < 1 or < 2 input points returns the input unchanged.
inline std::vector<vec2> catmullRomSpline(const std::vector<vec2>& points, int samplesPerSegment,
                                          bool closed = false) {
    const std::size_t n = points.size();
    if (samplesPerSegment < 1 || n < 2) {
        return points;
    }
    const std::size_t segCount = closed ? n : (n - 1);
    std::vector<vec2> out;
    out.reserve(segCount * static_cast<std::size_t>(samplesPerSegment) + 1);
    for (std::size_t seg = 0; seg < segCount; ++seg) {
        const vec2 p1 = points[seg];
        const vec2 p2 = points[(seg + 1) % n];
        const vec2 p0 = closed ? points[(seg + n - 1) % n] : points[seg == 0 ? 0 : seg - 1];
        const vec2 p3 =
            closed ? points[(seg + 2) % n] : points[seg + 2 < n ? seg + 2 : n - 1];
        for (int k = 0; k < samplesPerSegment; ++k) {
            const float w = static_cast<float>(k) / static_cast<float>(samplesPerSegment);
            out.push_back(cubicInterpolate<vec2>(p1, p2, p0, p3, w));
        }
    }
    if (!closed) {
        out.push_back(points.back()); // include the final endpoint (w=1 of the last segment)
    }
    return out;
}

// Clip segment a->b to the axis-aligned rectangle [rectMin, rectMax] using the Liang-Barsky algorithm.
// Returns true if any part of the segment lies inside the rect, writing the clipped endpoints into
// outA/outB (both on the original line, in a->b order); returns false and leaves the outputs untouched when
// the segment is entirely outside. This is viewport/scissor clipping for a single segment — the standard
// way to trim a debug line, laser sight, aim ray, or minimap trace to the visible bounds. It complements
// clipPolygon (Sutherland-Hodgman, which clips a filled polygon) and segmentIntersect (which needs a second
// segment): here the clip target is a rectangle, given as its min and max corners (e.g. Rect2 position and
// end()). Liang-Barsky solves the four edge parameters directly, so it is branch-light and allocation-free.
inline bool clipSegmentToRect(vec2 a, vec2 b, vec2 rectMin, vec2 rectMax, vec2& outA, vec2& outB) {
    // Normalize the corners so min really is the lower-left, tolerating a swapped rect.
    const float xmin = rectMin.x < rectMax.x ? rectMin.x : rectMax.x;
    const float xmax = rectMin.x < rectMax.x ? rectMax.x : rectMin.x;
    const float ymin = rectMin.y < rectMax.y ? rectMin.y : rectMax.y;
    const float ymax = rectMin.y < rectMax.y ? rectMax.y : rectMin.y;

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    float t0 = 0.0f; // entering parameter along a->b
    float t1 = 1.0f; // leaving parameter along a->b

    // Each edge gives a constraint p*t <= q; test and tighten [t0, t1].
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {a.x - xmin, xmax - a.x, a.y - ymin, ymax - a.y};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0f) {
            if (q[i] < 0.0f) {
                return false; // parallel to this edge and outside its slab
            }
            continue;
        }
        const float r = q[i] / p[i];
        if (p[i] < 0.0f) {
            if (r > t1) {
                return false; // enters after it already left
            }
            if (r > t0) {
                t0 = r; // later entry
            }
        } else {
            if (r < t0) {
                return false; // leaves before it entered
            }
            if (r < t1) {
                t1 = r; // earlier exit
            }
        }
    }

    outA = vec2(a.x + t0 * dx, a.y + t0 * dy);
    outB = vec2(a.x + t1 * dx, a.y + t1 * dy);
    return true;
}

// The minimum-AREA oriented bounding rectangle of a point set (rotating calipers). Unlike an axis-aligned
// box, this is the smallest ROTATED rectangle that encloses the points — the tight fit for a rotated
// sprite's hitbox, recovering an object's orientation from its silhouette, or snug packing. Toussaint's
// theorem guarantees the optimum has one side collinear with a convex-hull edge, so we build the hull and,
// for each edge direction, measure the bounding box in that frame and keep the smallest. Fields describe
// the rectangle by centre, its two (unit) axes and half-extents, its rotation angle, area, and 4 corners.
struct OrientedRect {
    vec2 center{0.0f, 0.0f};
    vec2 axisU{1.0f, 0.0f}; // unit direction of the "width" side
    vec2 axisV{0.0f, 1.0f}; // unit direction of the "height" side (perpendicular)
    float halfU = 0.0f;     // half-length along axisU
    float halfV = 0.0f;     // half-length along axisV
    float angle = 0.0f;     // rotation of axisU from +x (radians)
    float area = 0.0f;
    vec2 corners[4]{};
};

inline OrientedRect minAreaRect(const std::vector<vec2>& points) {
    OrientedRect best;
    if (points.empty()) {
        return best;
    }
    if (points.size() == 1) {
        best.center = points[0];
        best.corners[0] = best.corners[1] = best.corners[2] = best.corners[3] = points[0];
        return best;
    }

    std::vector<vec2> hull = convexHull(points);
    if (hull.size() < 2) {
        best.center = hull.empty() ? points[0] : hull[0];
        for (int i = 0; i < 4; ++i) {
            best.corners[i] = best.center;
        }
        return best;
    }

    float bestArea = -1.0f;
    float bestMinU = 0.0f, bestMaxU = 0.0f, bestMinV = 0.0f, bestMaxV = 0.0f;
    vec2 bestU(1.0f, 0.0f), bestV(0.0f, 1.0f);

    const std::size_t n = hull.size();
    for (std::size_t i = 0; i < n; ++i) {
        const vec2 a = hull[i];
        const vec2 b = hull[(i + 1) % n];
        vec2 u = b - a;
        const float len = std::sqrt(u.x * u.x + u.y * u.y);
        if (len < 1e-9f) {
            continue;
        }
        u = vec2(u.x / len, u.y / len);
        const vec2 v(-u.y, u.x); // perpendicular

        float minU = 0.0f, maxU = 0.0f, minV = 0.0f, maxV = 0.0f;
        bool first = true;
        for (const vec2& p : hull) {
            const float pu = p.x * u.x + p.y * u.y;
            const float pv = p.x * v.x + p.y * v.y;
            if (first) {
                minU = maxU = pu;
                minV = maxV = pv;
                first = false;
            } else {
                if (pu < minU) minU = pu;
                if (pu > maxU) maxU = pu;
                if (pv < minV) minV = pv;
                if (pv > maxV) maxV = pv;
            }
        }
        const float area = (maxU - minU) * (maxV - minV);
        if (bestArea < 0.0f || area < bestArea) {
            bestArea = area;
            bestU = u;
            bestV = v;
            bestMinU = minU;
            bestMaxU = maxU;
            bestMinV = minV;
            bestMaxV = maxV;
        }
    }

    const float cu = (bestMinU + bestMaxU) * 0.5f;
    const float cv = (bestMinV + bestMaxV) * 0.5f;
    best.center = bestU * cu + bestV * cv;
    best.axisU = bestU;
    best.axisV = bestV;
    best.halfU = (bestMaxU - bestMinU) * 0.5f;
    best.halfV = (bestMaxV - bestMinV) * 0.5f;
    best.angle = std::atan2(bestU.y, bestU.x);
    best.area = bestArea;
    best.corners[0] = best.center - bestU * best.halfU - bestV * best.halfV;
    best.corners[1] = best.center + bestU * best.halfU - bestV * best.halfV;
    best.corners[2] = best.center + bestU * best.halfU + bestV * best.halfV;
    best.corners[3] = best.center - bestU * best.halfU + bestV * best.halfV;
    return best;
}

} // namespace maz::math
