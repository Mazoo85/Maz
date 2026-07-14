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
// refinements.

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

} // namespace maz::math
