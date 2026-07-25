#pragma once

#include "maz/math/Geometry3D.hpp" // math::Aabb3 (overload)
#include "maz/math/Math.hpp"       // math::vec3, cross, dot

#include <cmath>

// maz::math TRIANGLE-AABB OVERLAP — does a triangle touch an axis-aligned box? The classic "is this triangle
// inside this voxel/cell?" test (Akenine-Möller, "Fast 3D Triangle-Box Overlap Testing", 2001), the primitive
// behind CONSERVATIVE voxelization (every cell a triangle grazes, not just where a ray happens to sample it),
// triangle binning into a uniform grid or octree/BVH, and coarse tri-vs-region culling. The engine had ray/box,
// box/box, and triangle/triangle, but not triangle/box. Implemented as the full separating-axis test: 3 box
// face normals, the triangle's own normal, and the 9 cross products of the box axes with the triangle edges —
// if any axis separates them they do not overlap, otherwise they do. Touching counts as overlap. A degenerate
// (zero-area) triangle still tests correctly as a segment/point against the box. Pure, header-only, headless.
namespace maz::math {

// True if triangle (v0,v1,v2) overlaps the box centred at `boxCenter` with half-extents `boxHalf` (>= 0).
inline bool triangleIntersectsAabb(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& boxCenter,
                                   const vec3& boxHalf) {
    // Triangle relative to the box centre.
    const vec3 t0 = v0 - boxCenter, t1 = v1 - boxCenter, t2 = v2 - boxCenter;
    const vec3 e0 = t1 - t0, e1 = t2 - t1, e2 = t0 - t2; // edges

    // Separating-axis test for one candidate axis L: separated if the triangle's projection interval and the
    // box's [-rad, rad] interval do not overlap. A near-zero axis (e.g. an edge parallel to a box axis) can't
    // separate, so it is skipped.
    auto separates = [&](const vec3& L) {
        if (dot(L, L) < 1e-20f) return false;
        const float p0 = dot(L, t0), p1 = dot(L, t1), p2 = dot(L, t2);
        const float mn = std::min(p0, std::min(p1, p2));
        const float mx = std::max(p0, std::max(p1, p2));
        const float rad = boxHalf.x * std::fabs(L.x) + boxHalf.y * std::fabs(L.y) + boxHalf.z * std::fabs(L.z);
        return mn > rad || mx < -rad;
    };

    // 9 axes: cross(box axis, triangle edge).
    const vec3 ax(1, 0, 0), ay(0, 1, 0), az(0, 0, 1);
    const vec3 edges[3] = {e0, e1, e2};
    for (const vec3& e : edges) {
        if (separates(cross(ax, e))) return false;
        if (separates(cross(ay, e))) return false;
        if (separates(cross(az, e))) return false;
    }
    // 3 axes: the box face normals (equivalent to comparing the triangle's AABB against the box).
    if (separates(ax) || separates(ay) || separates(az)) return false;
    // 1 axis: the triangle's face normal (the plane/box test).
    if (separates(cross(e0, e1))) return false;

    return true; // no separating axis → they overlap
}

// Overload taking a math::Aabb3.
inline bool triangleIntersectsAabb(const vec3& v0, const vec3& v1, const vec3& v2, const Aabb3& box) {
    return triangleIntersectsAabb(v0, v1, v2, box.center(), box.half());
}

} // namespace maz::math
