#pragma once

#include "maz/math/Math.hpp"      // vec3, cross
#include "maz/math/VectorOps.hpp" // dot

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math Newell's method for a 3D polygon — the robust normal, area, and area-weighted centroid of an
// arbitrary planar (or nearly-planar) polygon with any number of vertices. The naive "cross two edges" face
// normal fails on n-gons: pick a near-collinear vertex pair and it collapses, and a slightly non-planar
// polygon has no single edge-pair that represents the whole face. Newell's method sums a signed contribution
// over EVERY edge, so it always yields a stable, area-weighted normal (its magnitude is exactly twice the
// polygon area) and is the standard way engines compute face normals for lightmap/collision meshes, CSG,
// and importers. The engine only had triangle face normals; this handles quads and general polygons. Godot
// exposes no such helper. Header-only, std-only, deterministic.
namespace maz::math {

struct PolygonInfo {
    bool valid = false;
    vec3 normal{0.0f}; // unit face normal (CCW winding gives the right-hand normal); zero if degenerate
    float area = 0.0f; // polygon area (always >= 0)
    vec3 centroid{0.0f};
};

// The raw Newell vector: sum over edges of the cross-like term. Its length is twice the polygon area and its
// direction is the (unnormalised) face normal. Exposed for callers that want area-weighted normals directly.
inline vec3 newellVector(const std::vector<vec3>& poly) {
    vec3 n(0.0f);
    const std::size_t m = poly.size();
    if (m < 3) {
        return n;
    }
    for (std::size_t i = 0; i < m; ++i) {
        const vec3& a = poly[i];
        const vec3& b = poly[(i + 1) % m];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n * 0.5f; // so |n| == area and 2*n is the classic Newell sum; normal direction unchanged
}

// Full polygon info: unit normal, area, and the area-weighted centroid (correct for convex AND simple concave
// polygons, via signed fan triangles relative to the face normal). `valid` is false for < 3 vertices or a
// degenerate (zero-area) polygon.
inline PolygonInfo polygonInfo3D(const std::vector<vec3>& poly) {
    PolygonInfo out;
    const std::size_t m = poly.size();
    if (m < 3) {
        return out;
    }
    const vec3 nv = newellVector(poly); // |nv| == area, direction == face normal
    out.area = std::sqrt(dot(nv, nv));
    if (out.area < 1e-12f) {
        return out;
    }
    out.normal = nv * (1.0f / out.area);
    // Area-weighted centroid via signed fan triangles from poly[0].
    vec3 acc(0.0f);
    float areaSum = 0.0f;
    const vec3& p0 = poly[0];
    for (std::size_t i = 1; i + 1 < m; ++i) {
        const vec3 e1 = poly[i] - p0;
        const vec3 e2 = poly[i + 1] - p0;
        const float triSigned = 0.5f * dot(out.normal, cross(e1, e2)); // signed area of triangle (p0,pi,pi+1)
        const vec3 triCentroid = (p0 + poly[i] + poly[i + 1]) * (1.0f / 3.0f);
        acc = acc + triCentroid * triSigned;
        areaSum += triSigned;
    }
    out.centroid = std::fabs(areaSum) > 1e-12f ? acc * (1.0f / areaSum) : p0;
    out.valid = true;
    return out;
}

// Convenience: the robust unit face normal (Newell). Returns +Z for a degenerate polygon.
inline vec3 polygonNormal3D(const std::vector<vec3>& poly) {
    const PolygonInfo info = polygonInfo3D(poly);
    return info.valid ? info.normal : vec3(0.0f, 0.0f, 1.0f);
}

// Convenience: the polygon area.
inline float polygonArea3D(const std::vector<vec3>& poly) { return std::sqrt(dot(newellVector(poly), newellVector(poly))); }

} // namespace maz::math
