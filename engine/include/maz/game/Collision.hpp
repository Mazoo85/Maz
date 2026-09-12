#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace maz::game {

// Axis-aligned bounding box.
struct Aabb {
    math::vec3 min;
    math::vec3 max;

    static Aabb fromCenterSize(const math::vec3& center, const math::vec3& size) {
        const math::vec3 h = size * 0.5f;
        return Aabb{center - h, center + h};
    }

    bool overlaps(const Aabb& o) const {
        return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y &&
               min.z < o.max.z && max.z > o.min.z;
    }
};

// Result of a ray query. `hit` is false when nothing was struck (index/ t/point undefined then).
struct RayHit {
    bool hit = false;
    float t = 0.0f;       // distance along the ray to the entry point
    math::vec3 point{};   // world-space hit point (origin + dir * t)
    uint32_t index = 0;   // index of the struck box in the queried list
};

// Ray vs a single AABB (slab method). Returns the entry distance if the ray starting at `origin`
// heading along (unit) `dir` enters the box within [0, maxDist], or a miss. Handles the ray
// starting inside the box (t clamped to 0).
inline RayHit raycastAabb(const math::vec3& origin, const math::vec3& dir, const Aabb& box,
                          float maxDist = 1e30f) {
    float tmin = 0.0f;
    float tmax = maxDist;
    for (int a = 0; a < 3; ++a) {
        const float o = origin[a];
        const float d = dir[a];
        if (std::abs(d) < 1e-8f) {
            if (o < box.min[a] || o > box.max[a]) {
                return RayHit{}; // parallel to this slab and outside it
            }
        } else {
            const float inv = 1.0f / d;
            float t1 = (box.min[a] - o) * inv;
            float t2 = (box.max[a] - o) * inv;
            if (t1 > t2) {
                const float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            tmin = t1 > tmin ? t1 : tmin;
            tmax = t2 < tmax ? t2 : tmax;
            if (tmin > tmax) {
                return RayHit{};
            }
        }
    }
    RayHit r;
    r.hit = true;
    r.t = tmin;
    r.point = origin + dir * tmin;
    return r;
}

// Ray vs a list of AABBs: returns the nearest hit (smallest t) and its index, or a miss.
inline RayHit raycast(const math::vec3& origin, const math::vec3& dir,
                      const std::vector<Aabb>& boxes, float maxDist = 1e30f) {
    RayHit best;
    float bestT = std::numeric_limits<float>::max();
    for (uint32_t i = 0; i < boxes.size(); ++i) {
        const RayHit h = raycastAabb(origin, dir, boxes[i], maxDist);
        if (h.hit && h.t < bestT) {
            bestT = h.t;
            best = h;
            best.index = i;
        }
    }
    return best;
}

// Move a box (centered at `pos`, of `halfExtents`) by `delta`, resolving against `solids` one
// axis at a time so it slides along surfaces instead of sticking. Returns the new center.
inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,
                            const std::vector<Aabb>& solids) {
    const int axes[3] = {0, 2, 1}; // x, then z, then y
    for (int i = 0; i < 3; ++i) {
        const int a = axes[i];
        pos[a] += delta[a];
        Aabb box{pos - halfExtents, pos + halfExtents};
        for (const Aabb& s : solids) {
            if (!box.overlaps(s)) {
                continue;
            }
            if (delta[a] > 0.0f) {
                pos[a] = s.min[a] - halfExtents[a];
            } else if (delta[a] < 0.0f) {
                pos[a] = s.max[a] + halfExtents[a];
            }
            box = Aabb{pos - halfExtents, pos + halfExtents};
        }
    }
    return pos;
}

} // namespace maz::game
