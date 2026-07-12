#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::render {

// 2D polyline stroking — Godot's Line2D. Maz can FILL a convex polygon (drawConvexPolygon), but a
// polyline is a *stroke*: a path of points thickened to a ribbon of a given WIDTH, with the corners
// (JOINTS) and the two ends (CAPS) shaped so the ribbon reads as one continuous stroke — the primitive
// behind trails, drawn curves, graphs, outlines, and lightning. This turns a point list into a triangle
// soup (groups of three math::vec2) that any 2D fill path can draw; it is pure geometry (no renderer
// dependency, no allocation beyond the output), so it unit-tests headlessly and is deterministic.

enum class JointMode {
    Miter, // extend the outer edges to their intersection (falls back to Bevel past the miter limit)
    Bevel, // a single flat triangle across the outer gap
    Round, // a fan of triangles filling the outer arc
};

enum class CapMode {
    None, // stop flat at the endpoint
    Box,  // extend the ribbon half a width past the endpoint
    Round, // a semicircular fan at the endpoint
};

struct PolylineStyle {
    float width = 4.0f;
    JointMode joint = JointMode::Miter;
    CapMode cap = CapMode::None;
    bool closed = false;      // treat the path as a loop (joins last→first, ignores caps)
    float miterLimit = 4.0f;  // max miter length in half-widths before falling back to bevel
};

namespace detail {

inline math::vec2 perp(math::vec2 d) { return math::vec2(-d.y, d.x); } // 90° CCW (points to the left of d)

inline math::vec2 normalized(math::vec2 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    return len > 1e-6f ? math::vec2(v.x / len, v.y / len) : math::vec2(0.0f, 0.0f);
}

inline void emitTri(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c) {
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
}

inline void emitQuad(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c, math::vec2 d) {
    emitTri(out, a, b, c);
    emitTri(out, a, c, d);
}

// Fan from `center` sweeping the arc from `a` to `b` (both at radius from center), taking the arc that
// passes through direction `through`. Subdivided into deterministic steps by angle.
inline void emitFan(std::vector<math::vec2>& out, math::vec2 center, math::vec2 a, math::vec2 b,
                    math::vec2 through) {
    const math::vec2 ra = a - center;
    const math::vec2 rb = b - center;
    const float radius = std::sqrt(ra.x * ra.x + ra.y * ra.y);
    if (radius < 1e-6f) {
        return;
    }
    float a0 = std::atan2(ra.y, ra.x);
    float a1 = std::atan2(rb.y, rb.x);
    const float twoPi = 6.28318530718f;
    float delta = a1 - a0;
    while (delta <= -3.14159265f) {
        delta += twoPi;
    }
    while (delta > 3.14159265f) {
        delta -= twoPi;
    }
    // Pick the arc whose midpoint points toward `through`.
    const float mid = a0 + delta * 0.5f;
    if (through.x * std::cos(mid) + through.y * std::sin(mid) < 0.0f) {
        delta += (delta > 0.0f) ? -twoPi : twoPi;
    }
    const int steps = std::max(2, static_cast<int>(std::ceil(std::fabs(delta) / 0.4f)));
    math::vec2 prev = a;
    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float ang = a0 + delta * t;
        const math::vec2 p(center.x + std::cos(ang) * radius, center.y + std::sin(ang) * radius);
        emitTri(out, center, prev, p);
        prev = p;
    }
}

// Intersection of line (p0, dir d0) and line (p1, dir d1). Returns false if near-parallel.
inline bool lineIntersect(math::vec2 p0, math::vec2 d0, math::vec2 p1, math::vec2 d1, math::vec2& out) {
    const float denom = d0.x * d1.y - d0.y * d1.x;
    if (std::fabs(denom) < 1e-6f) {
        return false;
    }
    const float t = ((p1.x - p0.x) * d1.y - (p1.y - p0.y) * d1.x) / denom;
    out = math::vec2(p0.x + d0.x * t, p0.y + d0.y * t);
    return true;
}

} // namespace detail

// Build the stroke triangles for a polyline. Returns a flat triangle soup (size is a multiple of 3).
inline std::vector<math::vec2> buildPolyline(const std::vector<math::vec2>& pts, const PolylineStyle& style) {
    std::vector<math::vec2> out;
    const std::size_t n = pts.size();
    if (n < 2) {
        return out;
    }
    const float h = style.width * 0.5f;

    // Segment count: closed loops add the wrap segment (last→first).
    const std::size_t segCount = style.closed ? n : n - 1;

    // Segment bodies (a rectangle per segment).
    for (std::size_t i = 0; i < segCount; ++i) {
        const math::vec2 a = pts[i];
        const math::vec2 b = pts[(i + 1) % n];
        const math::vec2 d = detail::normalized(b - a);
        if (d.x == 0.0f && d.y == 0.0f) {
            continue;
        }
        const math::vec2 nrm = detail::perp(d);
        detail::emitQuad(out, a + nrm * h, b + nrm * h, b - nrm * h, a - nrm * h);
    }

    // Joints at interior vertices (all vertices when closed).
    const std::size_t firstJoint = style.closed ? 0 : 1;
    const std::size_t lastJoint = style.closed ? n : n - 1;
    for (std::size_t i = firstJoint; i < lastJoint; ++i) {
        const math::vec2 v = pts[i];
        const math::vec2 prev = pts[(i + n - 1) % n];
        const math::vec2 next = pts[(i + 1) % n];
        const math::vec2 d0 = detail::normalized(v - prev);
        const math::vec2 d1 = detail::normalized(next - v);
        const float cross = d0.x * d1.y - d0.y * d1.x;
        if (std::fabs(cross) < 1e-5f) {
            continue; // straight — no joint needed
        }
        const bool left = cross > 0.0f;                       // turning left → outer side is the right
        const float s = left ? -1.0f : 1.0f;                  // outer-normal multiplier
        const math::vec2 o0 = v + detail::perp(d0) * (h * s); // outer corner of the incoming segment
        const math::vec2 o1 = v + detail::perp(d1) * (h * s); // outer corner of the outgoing segment

        if (style.joint == JointMode::Round) {
            detail::emitFan(out, v, o0, o1, detail::normalized((o0 - v) + (o1 - v)));
        } else if (style.joint == JointMode::Miter) {
            math::vec2 apex;
            const bool ok = detail::lineIntersect(o0, d0, o1, d1, apex);
            const math::vec2 rel = ok ? (apex - v) : math::vec2(0.0f, 0.0f);
            const float miterLen = std::sqrt(rel.x * rel.x + rel.y * rel.y);
            if (ok && miterLen <= style.miterLimit * h) {
                detail::emitTri(out, v, o0, apex);
                detail::emitTri(out, v, apex, o1);
            } else {
                detail::emitTri(out, v, o0, o1); // fall back to bevel on a sharp turn
            }
        } else {
            detail::emitTri(out, v, o0, o1); // Bevel
        }
    }

    // Caps (open lines only).
    if (!style.closed && style.cap != CapMode::None) {
        const math::vec2 startDir = detail::normalized(pts[1] - pts[0]);
        const math::vec2 endDir = detail::normalized(pts[n - 1] - pts[n - 2]);
        struct End {
            math::vec2 p;
            math::vec2 outward;
        };
        const End ends[2] = {{pts[0], startDir * -1.0f}, {pts[n - 1], endDir}};
        for (const End& e : ends) {
            const math::vec2 nrm = detail::perp(e.outward);
            const math::vec2 a = e.p + nrm * h;
            const math::vec2 b = e.p - nrm * h;
            if (style.cap == CapMode::Box) {
                detail::emitQuad(out, a, a + e.outward * h, b + e.outward * h, b);
            } else { // Round
                detail::emitFan(out, e.p, a, b, e.outward);
            }
        }
    }

    return out;
}

} // namespace maz::render
