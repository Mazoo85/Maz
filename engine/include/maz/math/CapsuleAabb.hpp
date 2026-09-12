#pragma once

#include "maz/math/Geometry3D.hpp"      // math::Aabb3 (distanceSquared, intersectsSegment)
#include "maz/math/Math.hpp"            // vec3
#include "maz/math/SegmentDistance.hpp" // math::closestBetweenSegments

#include <algorithm>
#include <cmath>

// maz::math capsule-vs-AABB — the sibling of capsule-vs-triangle (M559) for the OTHER world representation:
// does a capsule (a segment `p0..p1` swept with radius `r` — the standard player/enemy body) touch an
// axis-aligned box? Tile/voxel/block worlds and simple prop colliders are AABBs, so a character controller
// walking such a world needs capsule↔AABB just as a mesh-level one needs capsule↔triangle. The engine had
// point↔AABB (`Aabb3::distanceSquared`), segment↔AABB pierce (`Aabb3::intersectsSegment`), sphere↔AABB
// (`Aabb3::intersectsSphere`) and segment↔segment (`closestBetweenSegments`) — but not the capsule↔AABB pair.
// This computes the exact minimum distance from the capsule's axis segment to the box and compares it to the
// radius: 0 when the segment pierces the box, otherwise the min over the two segment endpoints projected onto
// the box and the segment against each of the box's twelve edges (which, as in the triangle case, also
// captures a segment running parallel above a face). Pure vec3 math, header-only, deterministic.
namespace maz::math {

// Squared minimum distance between the segment p0..p1 and the axis-aligned box. 0 when they touch/cross.
inline float squaredDistanceSegmentAabb(const vec3& p0, const vec3& p1, const Aabb3& box) {
    if (box.intersectsSegment(p0, p1)) {
        return 0.0f;
    }
    // Endpoints projected onto the box (captures endpoint-over-face/edge/corner and the perpendicular case
    // where an endpoint sits directly above a face).
    float best = std::fmin(box.distanceSquared(p0), box.distanceSquared(p1));

    // The 8 corners and 12 edges of the box.
    const vec3 lo = box.min, hi = box.max;
    const vec3 corner[8] = {
        vec3(lo.x, lo.y, lo.z), vec3(hi.x, lo.y, lo.z), vec3(hi.x, hi.y, lo.z), vec3(lo.x, hi.y, lo.z),
        vec3(lo.x, lo.y, hi.z), vec3(hi.x, lo.y, hi.z), vec3(hi.x, hi.y, hi.z), vec3(lo.x, hi.y, hi.z),
    };
    static const int edge[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face (z = lo)
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face (z = hi)
        {0, 4}, {1, 5}, {2, 6}, {3, 7}, // vertical pillars
    };
    for (const auto& e : edge) {
        const float d = closestBetweenSegments(p0, p1, corner[e[0]], corner[e[1]]).distance;
        best = std::fmin(best, d * d);
    }
    return best;
}

// Does a capsule (segment p0..p1, radius r) overlap the axis-aligned box?
inline bool capsuleIntersectsAabb(const vec3& p0, const vec3& p1, float r, const Aabb3& box) {
    const float rr = (r < 0.0f ? 0.0f : r);
    return squaredDistanceSegmentAabb(p0, p1, box) <= rr * rr;
}

} // namespace maz::math
