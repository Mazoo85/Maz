#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::game {

// Arbitrary 2D CONVEX POLYGON collision via the Separating-Axis Theorem (SAT) — Godot's
// ConvexPolygonShape2D / CollisionPolygon2D. Physics2D already collides circles and (oriented) boxes, and
// it uses SAT internally for the box-box case, but there was no way to collide an ARBITRARY convex shape:
// a triangle, a pentagon, a hexagonal bumper, a hand-authored hull. This adds a standalone convex-polygon
// type plus the two queries every 2D game wants against it — "do these two shapes overlap, and if so which
// way and how far do I push to separate them (the minimum translation vector)?" and "is this point inside
// this shape?" — as pure geometry with no simulation or renderer dependency, so it unit-tests headlessly.
//
// SAT: two convex shapes are disjoint iff there exists a separating axis — a line onto which their
// projections don't overlap. The candidate axes are the face normals of both polygons. If every axis
// shows overlap, the shapes intersect, and the axis of MINIMUM overlap gives the MTV (push direction +
// penetration depth). Only valid for CONVEX polygons (decompose concave shapes into convex pieces first).

struct ConvexPoly2D {
    std::vector<math::vec2> points; // convex, either winding
};

struct SatHit2D {
    bool overlap = false;
    math::vec2 axis{0.0f, 0.0f}; // unit MTV axis, pointing from A toward B (move B by +axis*depth to separate)
    float depth = 0.0f;          // penetration depth along `axis`
    explicit operator bool() const { return overlap; }
};

namespace detail {

// Project a polygon onto a (unit) axis, returning [min,max] of the dot products.
inline void projectPoly(const ConvexPoly2D& p, const math::vec2& axis, float& mn, float& mx) {
    mn = 1e30f;
    mx = -1e30f;
    for (const math::vec2& v : p.points) {
        const float d = v.x * axis.x + v.y * axis.y;
        mn = d < mn ? d : mn;
        mx = d > mx ? d : mx;
    }
}

inline math::vec2 polyCenter(const ConvexPoly2D& p) {
    math::vec2 c(0.0f, 0.0f);
    for (const math::vec2& v : p.points) {
        c += v;
    }
    if (!p.points.empty()) {
        c /= static_cast<float>(p.points.size());
    }
    return c;
}

// Accumulate the minimum-overlap axis from one polygon's edge normals into `best`. Returns false if a
// separating axis is found (i.e. no overlap).
inline bool testAxes(const ConvexPoly2D& src, const ConvexPoly2D& a, const ConvexPoly2D& b,
                     float& bestDepth, math::vec2& bestAxis) {
    const std::size_t n = src.points.size();
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2 p0 = src.points[i];
        const math::vec2 p1 = src.points[(i + 1) % n];
        const math::vec2 edge = p1 - p0;
        math::vec2 axis(-edge.y, edge.x); // edge normal
        const float len = std::sqrt(axis.x * axis.x + axis.y * axis.y);
        if (len < 1e-9f) {
            continue;
        }
        axis /= len;
        float amn, amx, bmn, bmx;
        projectPoly(a, axis, amn, amx);
        projectPoly(b, axis, bmn, bmx);
        if (amx < bmn || bmx < amn) {
            return false; // separating axis -> no overlap
        }
        const float overlap = (amx < bmx ? amx : bmx) - (amn > bmn ? amn : bmn);
        if (overlap < bestDepth) {
            bestDepth = overlap;
            bestAxis = axis;
        }
    }
    return true;
}

} // namespace detail

// Overlap test between two convex polygons. On overlap, returns the MTV (unit axis A->B + depth).
inline SatHit2D satOverlap(const ConvexPoly2D& a, const ConvexPoly2D& b) {
    SatHit2D hit;
    if (a.points.size() < 3 || b.points.size() < 3) {
        return hit;
    }
    float bestDepth = 1e30f;
    math::vec2 bestAxis(0.0f, 0.0f);
    if (!detail::testAxes(a, a, b, bestDepth, bestAxis)) {
        return hit;
    }
    if (!detail::testAxes(b, a, b, bestDepth, bestAxis)) {
        return hit;
    }
    // Orient the MTV to point from A toward B.
    const math::vec2 d = detail::polyCenter(b) - detail::polyCenter(a);
    if (d.x * bestAxis.x + d.y * bestAxis.y < 0.0f) {
        bestAxis = -bestAxis;
    }
    hit.overlap = true;
    hit.axis = bestAxis;
    hit.depth = bestDepth;
    return hit;
}

// Is `pt` inside convex polygon `p`?  (Consistent sign of the cross product across every edge.)
inline bool polyContains(const ConvexPoly2D& p, const math::vec2& pt) {
    const std::size_t n = p.points.size();
    if (n < 3) {
        return false;
    }
    bool anyPos = false, anyNeg = false;
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2 p0 = p.points[i];
        const math::vec2 p1 = p.points[(i + 1) % n];
        const math::vec2 edge = p1 - p0;
        const math::vec2 rel = pt - p0;
        const float cross = edge.x * rel.y - edge.y * rel.x;
        if (cross > 1e-6f) {
            anyPos = true;
        } else if (cross < -1e-6f) {
            anyNeg = true;
        }
        if (anyPos && anyNeg) {
            return false; // on both sides of some edges -> outside a convex polygon
        }
    }
    return true;
}

// Build a regular n-gon centered at `center`, circumscribed radius `radius`, rotated by `rotation` rad.
inline ConvexPoly2D makeRegularPoly(const math::vec2& center, float radius, int sides, float rotation = 0.0f) {
    ConvexPoly2D poly;
    if (sides < 3) {
        sides = 3;
    }
    poly.points.reserve(static_cast<std::size_t>(sides));
    const float step = 6.28318530717958647692f / static_cast<float>(sides);
    for (int i = 0; i < sides; ++i) {
        const float a = rotation + step * static_cast<float>(i);
        poly.points.push_back(center + math::vec2(std::cos(a), std::sin(a)) * radius);
    }
    return poly;
}

// Build an oriented box (4 corners) centered at `center`, half-extents `half`, rotated `angle` rad.
inline ConvexPoly2D makeBoxPoly(const math::vec2& center, const math::vec2& half, float angle = 0.0f) {
    const float c = std::cos(angle), s = std::sin(angle);
    const math::vec2 ax(c, s), ay(-s, c);
    ConvexPoly2D poly;
    poly.points = {center - ax * half.x - ay * half.y, center + ax * half.x - ay * half.y,
                   center + ax * half.x + ay * half.y, center - ax * half.x + ay * half.y};
    return poly;
}

} // namespace maz::game
