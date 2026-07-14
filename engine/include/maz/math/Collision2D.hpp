#pragma once

// Convex polygon vs convex polygon overlap test via the Separating Axis Theorem
// (the Godot 2D convex-collision analog). Projects both polygons onto every edge
// normal of both and reports overlap unless some axis separates them. Inputs must
// be CONVEX (any winding); a concave polygon gives wrong results — run a convex
// decomposition first. Touching (a shared edge/vertex, intervals meeting at a
// point) counts as overlapping (inclusive). The axes are NOT normalized (the
// projection comparison is scale-invariant per axis). Composes maz::math vec2;
// pairs with iter54 convexHull2D (hull a point cloud, then test overlap).
// O(na*nb)-ish (na+nb axes, each projecting na+nb vertices). NOT a full physics
// contact solver — the minimum-translation vector and swept tests are future
// refinements. Also provides pointInPolygon: a crossing-number (even-odd) point
// containment test that — unlike convexPolygonsOverlap — works for ANY simple
// polygon (convex OR concave, any winding); a point exactly on the boundary
// (edge/vertex) has an unspecified result. Also provides segmentIntersection: a
// segment-segment single-point intersection test — returns the crossing point when
// two segments cross at one point, and returns false for parallel, collinear (even
// overlapping), or out-of-span pairs; endpoints are inclusive.

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <cstddef>
#include <vector>

namespace maz::math {

// Project a polygon's vertices onto `axis` and return the scalar interval
// [outMin, outMax] (each projection is dot(vertex, axis); the axis need not be
// unit-length — only the relative ordering matters for SAT).
inline void projectInterval(const std::vector<vec2>& poly, vec2 axis, float& outMin,
                            float& outMax) {
    float mn = dot(poly[0], axis);
    float mx = mn;
    for (std::size_t i = 1; i < poly.size(); ++i) {
        const float d = dot(poly[i], axis);
        mn = d < mn ? d : mn;
        mx = d > mx ? d : mx;
    }
    outMin = mn;
    outMax = mx;
}

// Test whether the two CONVEX polygons overlap via the Separating Axis Theorem.
// Returns true iff they overlap (touching counts as overlap). Both vertex lists
// may use either winding; a concave polygon must be convex-decomposed first.
inline bool convexPolygonsOverlap(const std::vector<vec2>& a, const std::vector<vec2>& b) {
    MAZ_ASSERT(a.size() >= 3 && b.size() >= 3,
               "convexPolygonsOverlap: polygons need >= 3 vertices");

    // For each edge of `poly`, take the edge normal as a candidate separating
    // axis, project both polygons onto it, and report true the moment their
    // intervals are strictly disjoint (a separating axis exists -> no overlap).
    auto checkAxes = [](const std::vector<vec2>& poly, const std::vector<vec2>& A,
                        const std::vector<vec2>& B) {
        const std::size_t n = poly.size();
        for (std::size_t i = 0; i < n; ++i) {
            const vec2 e = poly[(i + 1) % n] - poly[i];
            const vec2 axis = vec2(-e.y, e.x); // perpendicular; sign/magnitude irrelevant.

            float minA = 0.0f, maxA = 0.0f, minB = 0.0f, maxB = 0.0f;
            projectInterval(A, axis, minA, maxA);
            projectInterval(B, axis, minB, maxB);

            // STRICT `<`: intervals meeting at a point (touching) are NOT disjoint.
            if (maxA < minB || maxB < minA) {
                return true; // separating axis found.
            }
        }
        return false;
    };

    return !(checkAxes(a, a, b) || checkAxes(b, a, b));
}

// Point-in-polygon test via the crossing-number (even-odd) ray-casting rule: cast a ray
// in +x from `point` and count how many polygon edges it crosses — an odd count means the
// point is inside. Works for ANY simple polygon (convex OR concave, any winding). A point
// exactly on an edge or vertex is on the boundary and its result is not guaranteed (a
// documented ambiguity of the crossing-number rule). O(n).
inline bool pointInPolygon(vec2 point, const std::vector<vec2>& poly) {
    MAZ_ASSERT(poly.size() >= 3, "pointInPolygon: polygon needs >= 3 vertices");
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const vec2& a = poly[i];
        const vec2& b = poly[j];
        // Edge (a,b) straddles the horizontal line y == point.y, AND the crossing x is to
        // the right of point.x. The straddle test guarantees b.y != a.y, so the division is
        // safe (no divide-by-zero).
        if (((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)) {
            inside = !inside;
        }
    }
    return inside;
}

// Segment A (a0->a1) vs segment B (b0->b1) intersection. If the two segments cross at a
// single point, writes that point to outPoint and returns true; returns false if they are
// parallel, collinear (even if overlapping — a collinear overlap has no single point and is
// NOT reported), or simply don't meet within both segments' [0,1] spans. Endpoints are
// inclusive (a shared endpoint or a T-junction counts as an intersection). outPoint is only
// written when returning true.
inline bool segmentIntersection(vec2 a0, vec2 a1, vec2 b0, vec2 b1, vec2& outPoint) {
    const vec2 d1 = a1 - a0;
    const vec2 d2 = b1 - b0;
    // 2D scalar cross product.
    const float denom = d1.x * d2.y - d1.y * d2.x;
    // Parallel or collinear -> no single intersection point.
    if (denom < 1e-9f && denom > -1e-9f) {
        return false;
    }
    const vec2 r = b0 - a0;
    const float t = (r.x * d2.y - r.y * d2.x) / denom;   // param along A  (cross(r, d2)/denom)
    const float u = (r.x * d1.y - r.y * d1.x) / denom;   // param along B  (cross(r, d1)/denom)
    if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) {
        return false;  // intersection of the infinite lines lies outside one/both segments
    }
    outPoint = a0 + t * d1;
    return true;
}

} // namespace maz::math
