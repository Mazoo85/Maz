#pragma once

#include "maz/math/Math.hpp"

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
