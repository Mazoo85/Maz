#pragma once

#include "maz/game/Collision.hpp"
#include "maz/math/Math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// maz::game::Overlap3D — 3D sphere overlap/trigger queries and a swept-sphere cast against AABBs,
// the "what's touching this volume?" and "what does this moving ball hit first?" primitives a 3D
// game needs for triggers, pickups, character sweeps, and explosion/AoE checks. This complements
// the existing ray queries in game::Collision (raycast vs AABBs) with volume queries.
//
//   sphereVsAabb / sphereVsSphere — exact overlap tests (closest-point on the box for AABBs)
//   overlapSphere(...)            — every AABB a sphere touches (Godot's intersect_shape for a
//                                   SphereShape3D), the trigger/overlap query
//   sphereCast(...)               — the nearest AABB a sphere of radius r sweeping along a ray
//   hits,
//                                   with the contact distance
//
// The overlap tests are EXACT. sphereCast is CONSERVATIVE: it sweeps by ray-testing each AABB grown
// by the radius, which is exact when the sphere meets a face but treats box corners as square
// rather than rounded — so near a corner it can report contact slightly early (never late: it has
// no false negatives). That's the standard, safe first-pass sweep for character/projectile
// movement; a corner-exact refinement can layer on later. Header-only, deterministic, no GPU.
namespace maz::game {

struct Sphere3 {
    math::vec3 center{0.0f};
    float radius = 0.0f;
};

// Closest point on an AABB to an arbitrary point (component-wise clamp).
inline math::vec3 closestPointOnAabb(const math::vec3& p, const Aabb& box) {
    return math::vec3(std::clamp(p.x, box.min.x, box.max.x), std::clamp(p.y, box.min.y, box.max.y),
                      std::clamp(p.z, box.min.z, box.max.z));
}

// Exact sphere-vs-AABB overlap: the sphere touches the box iff its center is within radius of the
// box's closest point.
inline bool sphereVsAabb(const math::vec3& center, float radius, const Aabb& box) {
    const math::vec3 d = center - closestPointOnAabb(center, box);
    return glm::dot(d, d) <= radius * radius;
}

inline bool sphereVsSphere(const math::vec3& ca, float ra, const math::vec3& cb, float rb) {
    const math::vec3 d = ca - cb;
    const float rr = ra + rb;
    return glm::dot(d, d) <= rr * rr;
}

// Every AABB in `boxes` the sphere overlaps (indices). The trigger/overlap query.
inline std::vector<uint32_t> overlapSphere(const math::vec3& center, float radius,
                                           const std::vector<Aabb>& boxes) {
    std::vector<uint32_t> out;
    for (uint32_t i = 0; i < boxes.size(); ++i) {
        if (sphereVsAabb(center, radius, boxes[i])) {
            out.push_back(i);
        }
    }
    return out;
}

// Conservative swept-sphere cast: the nearest AABB a sphere of `radius` starting at `origin` and
// moving along `dir` first contacts, within `maxDist` (measured along dir; dir need not be unit).
// Each box is grown by `radius` and ray-tested — exact on faces, square (conservative) at corners.
inline RayHit sphereCast(const math::vec3& origin, const math::vec3& dir, float radius,
                         const std::vector<Aabb>& boxes,
                         float maxDist = std::numeric_limits<float>::max()) {
    RayHit best;
    float bestT = std::numeric_limits<float>::max();
    const math::vec3 r(radius);
    for (uint32_t i = 0; i < boxes.size(); ++i) {
        Aabb grown{boxes[i].min - r, boxes[i].max + r};
        const RayHit h = raycastAabb(origin, dir, grown, maxDist);
        if (h.hit && h.t < bestT) {
            bestT = h.t;
            best = h;
            best.index = i;
        }
    }
    return best;
}

} // namespace maz::game
