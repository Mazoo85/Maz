#pragma once

#include "maz/math/Math.hpp" // math::vec3, cross, dot

#include <cmath>
#include <utility> // std::swap

// maz::math TRIANGLE-TRIANGLE INTERSECTION — do two triangles in 3D touch or cross? This is the narrowphase
// primitive the engine was missing: it already has ray/triangle (Möller-Trumbore), segment/triangle, and
// closest-point-on-triangle, but not triangle-vs-triangle. It is the test behind mesh self-intersection QA
// ("select self-intersecting faces" in Blender/Godot mesh repair), mesh-vs-mesh overlap once a broadphase has
// paired candidate triangles, CSG/boolean preconditions, and cloth/soft-body collision. Uses Tomas Möller's
// "A Fast Triangle-Triangle Intersection Test" (1997): reject early when one triangle lies entirely on one side
// of the other's plane; otherwise the two planes meet in a line and each triangle cuts an interval on it — they
// intersect iff those intervals overlap. The coplanar case falls back to a 2D convex overlap (SAT). Touching
// (a shared edge or vertex, or one grazing the other) counts as intersecting, matching Möller — so callers doing
// self-intersection must skip edge/vertex-adjacent triangle pairs, which legitimately share geometry. Symmetric
// in its two arguments. Pure, header-only, headless.
namespace maz::math {

namespace detail {

// Project the three 2D points onto `axis` (ax, ay) and return the [min, max] extent.
inline void triProject2(const float p[3][2], float ax, float ay, float& mn, float& mx) {
    mn = mx = p[0][0] * ax + p[0][1] * ay;
    for (int i = 1; i < 3; ++i) {
        const float d = p[i][0] * ax + p[i][1] * ay;
        if (d < mn) mn = d;
        if (d > mx) mx = d;
    }
}

// Convex overlap of two 2D triangles via the separating-axis theorem over both triangles' edge normals.
// Touching (extents meeting exactly) counts as overlap.
inline bool triOverlap2(const float a[3][2], const float b[3][2]) {
    const float(*tris[2])[2] = {a, b};
    for (int t = 0; t < 2; ++t) {
        const float(*tri)[2] = tris[t];
        for (int e = 0; e < 3; ++e) {
            const int j = (e + 1) % 3;
            const float ex = tri[j][0] - tri[e][0];
            const float ey = tri[j][1] - tri[e][1];
            const float ax = -ey, ay = ex; // edge normal
            float amn, amx, bmn, bmx;
            triProject2(a, ax, ay, amn, amx);
            triProject2(b, ax, ay, bmn, bmx);
            if (amx < bmn || bmx < amn) return false; // found a separating axis
        }
    }
    return true;
}

// Coplanar triangle test: drop the dominant axis of the shared normal `n`, then 2D convex overlap.
inline bool coplanarTriTri(const vec3& n, const vec3& v0, const vec3& v1, const vec3& v2, const vec3& u0,
                           const vec3& u1, const vec3& u2) {
    const float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
    int i0, i1; // the two axes to keep (drop the largest normal component)
    if (ax > ay) {
        if (ax > az) { i0 = 1; i1 = 2; } else { i0 = 0; i1 = 1; }
    } else {
        if (az > ay) { i0 = 0; i1 = 1; } else { i0 = 0; i1 = 2; }
    }
    const float a[3][2] = {{v0[i0], v0[i1]}, {v1[i0], v1[i1]}, {v2[i0], v2[i1]}};
    const float b[3][2] = {{u0[i0], u0[i1]}, {u1[i0], u1[i1]}, {u2[i0], u2[i1]}};
    return triOverlap2(a, b);
}

// The parametric interval [i0, i1] where triangle (VV*, projections onto the intersection-line axis) crosses the
// line, given signed plane distances D* and their sign products. Arranged so the "lone" vertex (opposite sign)
// anchors the two edge crossings.
inline void triInterval(float VV0, float VV1, float VV2, float D0, float D1, float D2, float D0D1, float D0D2,
                        float& i0, float& i1) {
    if (D0D1 > 0.0f) { // D2 is the lone vertex
        i0 = VV2 + (VV0 - VV2) * D2 / (D2 - D0);
        i1 = VV2 + (VV1 - VV2) * D2 / (D2 - D1);
    } else if (D0D2 > 0.0f) { // D1 lone
        i0 = VV1 + (VV0 - VV1) * D1 / (D1 - D0);
        i1 = VV1 + (VV2 - VV1) * D1 / (D1 - D2);
    } else if (D1 * D2 > 0.0f || D0 != 0.0f) { // D0 lone
        i0 = VV0 + (VV1 - VV0) * D0 / (D0 - D1);
        i1 = VV0 + (VV2 - VV0) * D0 / (D0 - D2);
    } else if (D1 != 0.0f) {
        i0 = VV1 + (VV0 - VV1) * D1 / (D1 - D0);
        i1 = VV1 + (VV2 - VV1) * D1 / (D1 - D2);
    } else { // D2 != 0
        i0 = VV2 + (VV0 - VV2) * D2 / (D2 - D0);
        i1 = VV2 + (VV1 - VV2) * D2 / (D2 - D1);
    }
}

} // namespace detail

// True if triangles (v0,v1,v2) and (u0,u1,u2) intersect (touching counts). Degenerate (zero-area) inputs whose
// plane normal vanishes are treated as non-intersecting rather than throwing.
inline bool trianglesIntersect(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& u0, const vec3& u1,
                               const vec3& u2) {
    constexpr float eps = 1e-6f;

    // Plane of triangle 2; signed distances of triangle 1's vertices to it.
    vec3 n2 = cross(u1 - u0, u2 - u0);
    const float d2 = -dot(n2, u0);
    float du0 = dot(n2, v0) + d2, du1 = dot(n2, v1) + d2, du2 = dot(n2, v2) + d2;
    if (std::fabs(du0) < eps) du0 = 0.0f;
    if (std::fabs(du1) < eps) du1 = 0.0f;
    if (std::fabs(du2) < eps) du2 = 0.0f;
    const float du0du1 = du0 * du1, du0du2 = du0 * du2;
    if (du0du1 > 0.0f && du0du2 > 0.0f) return false; // triangle 1 entirely on one side of plane 2

    // Plane of triangle 1; signed distances of triangle 2's vertices to it.
    vec3 n1 = cross(v1 - v0, v2 - v0);
    const float d1 = -dot(n1, v0);
    float dv0 = dot(n1, u0) + d1, dv1 = dot(n1, u1) + d1, dv2 = dot(n1, u2) + d1;
    if (std::fabs(dv0) < eps) dv0 = 0.0f;
    if (std::fabs(dv1) < eps) dv1 = 0.0f;
    if (std::fabs(dv2) < eps) dv2 = 0.0f;
    const float dv0dv1 = dv0 * dv1, dv0dv2 = dv0 * dv2;
    if (dv0dv1 > 0.0f && dv0dv2 > 0.0f) return false; // triangle 2 entirely on one side of plane 1

    // Direction of the line where the two planes meet; its dominant axis is the projection axis.
    const vec3 dir = cross(n1, n2);
    float mx = std::fabs(dir.x);
    int index = 0;
    if (std::fabs(dir.y) > mx) { mx = std::fabs(dir.y); index = 1; }
    if (std::fabs(dir.z) > mx) { mx = std::fabs(dir.z); index = 2; }
    if (mx < eps) { // planes parallel → coplanar, OR a degenerate triangle whose normal vanished
        constexpr float degEps = 1e-12f;
        if (dot(n1, n1) < degEps || dot(n2, n2) < degEps) return false; // a zero-area triangle never hits
        return detail::coplanarTriTri(n1, v0, v1, v2, u0, u1, u2);
    }

    // Intervals each triangle cuts on the intersection line; intersect iff they overlap.
    const float vp0 = v0[index], vp1 = v1[index], vp2 = v2[index];
    const float up0 = u0[index], up1 = u1[index], up2 = u2[index];
    float i1[2], i2[2];
    detail::triInterval(vp0, vp1, vp2, du0, du1, du2, du0du1, du0du2, i1[0], i1[1]);
    detail::triInterval(up0, up1, up2, dv0, dv1, dv2, dv0dv1, dv0dv2, i2[0], i2[1]);
    if (i1[0] > i1[1]) std::swap(i1[0], i1[1]);
    if (i2[0] > i2[1]) std::swap(i2[0], i2[1]);
    return !(i1[1] < i2[0] || i2[1] < i1[0]);
}

} // namespace maz::math
