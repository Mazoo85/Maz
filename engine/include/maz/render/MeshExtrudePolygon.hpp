#pragma once

#include "maz/math/Math.hpp"           // math::vec2, vec3
#include "maz/render/PolyTriangulate.hpp" // triangulatePolygon, polygonSignedArea2
#include "maz/render/Shapes.hpp"        // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render LINEAR EXTRUDE / PRISM FROM A POLYGON — take any flat 2D shape and give it thickness, turning the
// outline into a solid 3D block. Draw a star, a gear, a heart, a letter of the alphabet, a company logo, an arrow,
// an L-shaped room footprint, or a staircase side-profile as a list of 2D points, and this stamps it out into a
// prism `depth` units thick. It is Godot's CSGPolygon3D in Depth mode / Blender's "extrude region" / the classic
// CAD linear-extrude, and the fastest way to make chunky 3D text, coins and medals, extruded signage, cookie-cutter
// props, pipes with a fancy cross-section, or blocky level geometry from a hand-drawn footprint. The flat shape is
// laid in the XY plane and pushed along Z, centred so it runs from z = -depth/2 to z = +depth/2. Two end caps
// (triangulated with the engine's ear-clipping `triangulatePolygon`) plus one quad wall per outline edge make a
// closed, solid prism. Header-only, deterministic, headless — pure CPU geometry.
//
// Scope note (honest): the outline must be a SIMPLE polygon — no self-crossings and no holes (a donut needs a
// separate hole-aware path). Winding is auto-normalised, so either clockwise or counter-clockwise input works. The
// mesh is UNWELDED with flat per-face normals, so the caps and side walls read as crisp hard edges (ideal for a
// faceted prism); it is a closed, consistently-wound solid (signed volume == area x depth). Fewer than 3 points, or
// a zero-area outline, yields an empty mesh.
namespace maz::render {

// Extrude the simple polygon `poly` (XY) into a prism `depth` thick along Z, centred on z=0.
inline shapes::MeshData extrudePolygon(const std::vector<math::vec2>& poly, float depth) {
    shapes::MeshData out;
    if (poly.size() < 3 || std::fabs(depth) < 1e-20f) return out;

    if (polygonArea(poly) < 1e-12f) return out; // zero-area / collinear outline
    // Normalise to CCW so cap/wall winding is deterministic.
    std::vector<math::vec2> p = poly;
    if (polygonSignedArea2(p) < 0.0f) {
        for (std::size_t i = 0; i < p.size() / 2; ++i) std::swap(p[i], p[p.size() - 1 - i]);
    }
    const std::vector<std::uint32_t> capTris = triangulatePolygon(p);
    if (capTris.empty()) return out; // degenerate / zero-area outline

    const std::size_t n = p.size();
    const float zf = -depth * 0.5f, zb = depth * 0.5f;
    auto push = [&](float x, float y, float z, float nx, float ny, float nz) {
        MeshVertex v{};
        v.px = x; v.py = y; v.pz = z;
        v.nx = nx; v.ny = ny; v.nz = nz;
        v.r = v.g = v.b = 1.0f;
        v.u = x; v.v = y;
        out.vertices.push_back(v);
    };
    auto face = [&](const math::vec3& a, const math::vec3& b, const math::vec3& c, float nx, float ny, float nz) {
        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        push(a.x, a.y, a.z, nx, ny, nz); push(b.x, b.y, b.z, nx, ny, nz); push(c.x, c.y, c.z, nx, ny, nz);
        out.indices.push_back(base); out.indices.push_back(base + 1); out.indices.push_back(base + 2);
    };

    // Caps: back at zb faces +Z (triangulator gives CCW/+Z); front at zf faces -Z (reversed winding).
    for (std::size_t t = 0; t < capTris.size(); t += 3) {
        const math::vec2& a = p[capTris[t]];
        const math::vec2& b = p[capTris[t + 1]];
        const math::vec2& c = p[capTris[t + 2]];
        face(math::vec3(a.x, a.y, zb), math::vec3(b.x, b.y, zb), math::vec3(c.x, c.y, zb), 0, 0, 1);   // back +Z
        face(math::vec3(a.x, a.y, zf), math::vec3(c.x, c.y, zf), math::vec3(b.x, b.y, zf), 0, 0, -1);  // front -Z
    }

    // Side walls: one quad per outline edge, outward normal (dy,-dx) for CCW winding.
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2& q0 = p[i];
        const math::vec2& q1 = p[(i + 1) % n];
        const float dx = q1.x - q0.x, dy = q1.y - q0.y;
        const float nl = std::sqrt(dx * dx + dy * dy);
        const float nx = nl > 1e-12f ? dy / nl : 0.0f, ny = nl > 1e-12f ? -dx / nl : 0.0f;
        const math::vec3 fA(q0.x, q0.y, zf), fB(q1.x, q1.y, zf), bA(q0.x, q0.y, zb), bB(q1.x, q1.y, zb);
        face(fA, bB, bA, nx, ny, 0.0f);
        face(fA, fB, bB, nx, ny, 0.0f);
    }
    return out;
}

// Tapered extrude: like `extrudePolygon`, but the top cap (z=+depth/2) is scaled by `topScale` about the polygon's
// centroid — a truncated pyramid / frustum from any 2D shape (a plinth, a building, a stub, a bevelled block). At
// topScale=1 this is a straight prism; below 1 it tapers inward toward the top; above 1 it flares out. The slanted
// side walls get true flat per-face normals computed from their geometry. `topScale` is clamped to a small positive
// minimum (a true point apex should use a cone/pyramid tool). Closed solid; volume follows the prismatoid rule.
inline shapes::MeshData extrudePolygonScaled(const std::vector<math::vec2>& poly, float depth, float topScale) {
    shapes::MeshData out;
    if (poly.size() < 3 || std::fabs(depth) < 1e-20f) return out;
    if (polygonArea(poly) < 1e-12f) return out;
    std::vector<math::vec2> p = poly;
    if (polygonSignedArea2(p) < 0.0f) {
        for (std::size_t i = 0; i < p.size() / 2; ++i) std::swap(p[i], p[p.size() - 1 - i]);
    }
    const std::vector<std::uint32_t> capTris = triangulatePolygon(p);
    if (capTris.empty()) return out;

    const std::size_t n = p.size();
    float s = topScale;
    if (s < 1e-3f) s = 1e-3f;
    const float zf = -depth * 0.5f, zb = depth * 0.5f;

    // Polygon centroid (vertex average) — the taper pivots here.
    float cx = 0.0f, cy = 0.0f;
    for (const math::vec2& q : p) { cx += q.x; cy += q.y; }
    cx /= static_cast<float>(n); cy /= static_cast<float>(n);
    auto top = [&](const math::vec2& q) { return math::vec2(cx + (q.x - cx) * s, cy + (q.y - cy) * s); };

    auto push = [&](const math::vec3& v, const math::vec3& nrm) {
        MeshVertex mv{};
        mv.px = v.x; mv.py = v.y; mv.pz = v.z;
        mv.nx = nrm.x; mv.ny = nrm.y; mv.nz = nrm.z;
        mv.r = mv.g = mv.b = 1.0f;
        mv.u = v.x; mv.v = v.y;
        out.vertices.push_back(mv);
    };
    auto face = [&](const math::vec3& a, const math::vec3& b, const math::vec3& c, const math::vec3& nrm) {
        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        push(a, nrm); push(b, nrm); push(c, nrm);
        out.indices.push_back(base); out.indices.push_back(base + 1); out.indices.push_back(base + 2);
    };
    auto flatNormal = [](const math::vec3& a, const math::vec3& b, const math::vec3& c) {
        const math::vec3 cr = math::cross(b - a, c - a);
        const float l = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
        return l > 1e-20f ? math::vec3(cr.x / l, cr.y / l, cr.z / l) : math::vec3(0, 0, 0);
    };

    // Caps: back (scaled) faces +Z; front faces -Z (reversed).
    for (std::size_t t = 0; t < capTris.size(); t += 3) {
        const math::vec2 a = p[capTris[t]], b = p[capTris[t + 1]], c = p[capTris[t + 2]];
        const math::vec2 ta = top(a), tb = top(b), tc = top(c);
        face(math::vec3(ta.x, ta.y, zb), math::vec3(tb.x, tb.y, zb), math::vec3(tc.x, tc.y, zb), math::vec3(0, 0, 1));
        face(math::vec3(a.x, a.y, zf), math::vec3(c.x, c.y, zf), math::vec3(b.x, b.y, zf), math::vec3(0, 0, -1));
    }

    // Slanted side walls: flat per-face normals from the actual quad geometry.
    for (std::size_t i = 0; i < n; ++i) {
        const math::vec2 q0 = p[i], q1 = p[(i + 1) % n];
        const math::vec2 t0 = top(q0), t1 = top(q1);
        const math::vec3 fA(q0.x, q0.y, zf), fB(q1.x, q1.y, zf), bA(t0.x, t0.y, zb), bB(t1.x, t1.y, zb);
        face(fA, bB, bA, flatNormal(fA, bB, bA));
        face(fA, fB, bB, flatNormal(fA, fB, bB));
    }
    return out;
}

} // namespace maz::render
