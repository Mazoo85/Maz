#pragma once

#include "maz/math/Math.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::render {

// Ear-clipping triangulation of a SIMPLE polygon — the geometry behind Godot's Polygon2D fill.
//
// Maz can already FILL a *convex* polygon (drawConvexPolygon triangulates it as a fan from vertex 0),
// but a fan only reads correctly when every interior angle is < 180 deg. Feed it a CONCAVE outline (a
// star, an arrow, an L / C / comb shape) and the fan spills triangles outside the shape. Godot's
// Polygon2D handles arbitrary simple polygons by triangulating them properly; this closes that gap.
//
// triangulatePolygon() runs the classic O(n^2) ear-clipping algorithm: repeatedly find a "convex ear"
// (a vertex whose triangle with its two neighbours points outward and contains no other vertex) and
// snip it off, until a single triangle remains. It works on any simple polygon (no self-intersections,
// no holes) of EITHER winding — the winding is detected via signed area and normalised to CCW so the
// convexity test is consistent. The result is a flat list of vertex INDICES into `poly`, three per
// triangle, each triangle wound the same way as the (normalised CCW) input. Every triangle is convex,
// so it can be drawn by the existing convex-fill path. Pure geometry: no renderer dependency, no
// allocation beyond the output, deterministic and headlessly unit-testable.

// Twice the signed area of triangle (a, b, c). Positive when a->b->c turns counter-clockwise.
inline float triSignedArea2(const math::vec2& a, const math::vec2& b, const math::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

// Twice the signed area of a polygon (the shoelace sum). Positive for CCW winding, negative for CW.
inline float polygonSignedArea2(const std::vector<math::vec2>& poly) {
    const std::size_t n = poly.size();
    if (n < 3) {
        return 0.0f;
    }
    float acc = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2& a = poly[i];
        const math::vec2& b = poly[(i + 1) % n];
        acc += a.x * b.y - b.x * a.y;
    }
    return acc;
}

// Absolute area of a simple polygon.
inline float polygonArea(const std::vector<math::vec2>& poly) {
    const float s = polygonSignedArea2(poly);
    return (s < 0.0f ? -s : s) * 0.5f;
}

// True when p lies inside (or on the boundary of) the CCW triangle (a, b, c).
inline bool pointInTriangle(const math::vec2& p, const math::vec2& a, const math::vec2& b,
                            const math::vec2& c) {
    const float d1 = triSignedArea2(a, b, p);
    const float d2 = triSignedArea2(b, c, p);
    const float d3 = triSignedArea2(c, a, p);
    // For a CCW triangle every cross product is >= 0 when p is inside; a mix of signs means outside.
    const bool hasNeg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNeg && hasPos);
}

inline std::vector<std::uint32_t> triangulatePolygon(const std::vector<math::vec2>& poly) {
    std::vector<std::uint32_t> tris;
    const std::size_t n = poly.size();
    if (n < 3) {
        return tris;
    }
    tris.reserve((n - 2) * 3);

    // Working ring of vertex indices, normalised to CCW so triSignedArea2 > 0 means "convex corner".
    std::vector<std::uint32_t> ring(n);
    if (polygonSignedArea2(poly) >= 0.0f) {
        for (std::size_t i = 0; i < n; ++i) {
            ring[i] = static_cast<std::uint32_t>(i);
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            ring[i] = static_cast<std::uint32_t>(n - 1 - i);
        }
    }

    std::size_t remaining = n;
    std::size_t cursor = 0;
    std::size_t sinceClip = 0; // consecutive vertices tried with no ear found
    while (remaining > 3) {
        // A full pass with no ear removed means the polygon is degenerate/self-intersecting; bail with
        // whatever has been emitted rather than looping forever.
        if (sinceClip >= remaining) {
            break;
        }
        const std::size_t prev = (cursor + remaining - 1) % remaining;
        const std::size_t next = (cursor + 1) % remaining;
        const math::vec2& a = poly[ring[prev]];
        const math::vec2& b = poly[ring[cursor]];
        const math::vec2& c = poly[ring[next]];

        bool ear = false;
        if (triSignedArea2(a, b, c) > 0.0f) { // convex corner
            ear = true;
            for (std::size_t k = 0; k < remaining; ++k) {
                if (k == prev || k == cursor || k == next) {
                    continue;
                }
                if (pointInTriangle(poly[ring[k]], a, b, c)) {
                    ear = false;
                    break;
                }
            }
        }

        if (ear) {
            tris.push_back(ring[prev]);
            tris.push_back(ring[cursor]);
            tris.push_back(ring[next]);
            ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(cursor));
            --remaining;
            sinceClip = 0;
            if (cursor >= remaining) {
                cursor = 0;
            }
        } else {
            cursor = (cursor + 1) % remaining;
            ++sinceClip;
        }
    }
    if (remaining == 3) {
        tris.push_back(ring[0]);
        tris.push_back(ring[1]);
        tris.push_back(ring[2]);
    }
    return tris;
}

} // namespace maz::render
