#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

namespace maz::game {

// Area2D — a sensor / trigger region. Unlike a rigid body (Physics2D) it pushes nothing; it just reports
// which things OVERLAP it and fires ENTER / EXIT as they cross its boundary. This is Godot's Area2D, and
// it's how nearly every game does pickups, hurt/hit boxes, checkpoints, doorways, and water / wind /
// gravity zones. It's pure geometry (circle & axis-aligned box overlap) plus set-diffing (this frame's
// members vs last frame's), so it's deterministic and unit-tests headlessly.

struct Area2D {
    enum Shape { Circle, Box };
    int shape = Circle;
    math::vec2 pos{0.0f, 0.0f};
    float radius = 1.0f;       // Circle
    math::vec2 half{1.0f, 1.0f}; // Box half-extents (axis-aligned)

    math::vec2 boundsMin() const {
        return shape == Box ? pos - half : math::vec2(pos.x - radius, pos.y - radius);
    }
    math::vec2 boundsMax() const {
        return shape == Box ? pos + half : math::vec2(pos.x + radius, pos.y + radius);
    }

    bool containsPoint(math::vec2 p) const {
        if (shape == Box) {
            return p.x >= pos.x - half.x && p.x <= pos.x + half.x && p.y >= pos.y - half.y &&
                   p.y <= pos.y + half.y;
        }
        const math::vec2 d = p - pos;
        return glm::dot(d, d) <= radius * radius;
    }
};

namespace detail {
inline math::vec2 closestOnBox(math::vec2 p, math::vec2 center, math::vec2 half) {
    return math::vec2(glm::clamp(p.x, center.x - half.x, center.x + half.x),
                      glm::clamp(p.y, center.y - half.y, center.y + half.y));
}
} // namespace detail

// Do two areas overlap? Circle/circle by centre distance, box/box by AABB, circle/box by closest point.
// Touching exactly (zero penetration) counts as NOT overlapping, so an object grazing the edge doesn't
// flicker in and out.
inline bool overlaps(const Area2D& a, const Area2D& b) {
    if (a.shape == Area2D::Circle && b.shape == Area2D::Circle) {
        const math::vec2 d = b.pos - a.pos;
        const float rr = a.radius + b.radius;
        return glm::dot(d, d) < rr * rr;
    }
    if (a.shape == Area2D::Box && b.shape == Area2D::Box) {
        return std::fabs(b.pos.x - a.pos.x) < (a.half.x + b.half.x) &&
               std::fabs(b.pos.y - a.pos.y) < (a.half.y + b.half.y);
    }
    // One circle, one box: closest point on the box to the circle centre.
    const Area2D& circle = a.shape == Area2D::Circle ? a : b;
    const Area2D& box = a.shape == Area2D::Circle ? b : a;
    const math::vec2 c = detail::closestOnBox(circle.pos, box.pos, box.half);
    const math::vec2 d = circle.pos - c;
    return glm::dot(d, d) < circle.radius * circle.radius;
}

// Tracks which ids are inside an area across frames and reports the transitions. Feed the CURRENT set of
// overlapping ids each frame; `entered`/`exited` come back with the newly-inside and newly-outside ids
// (Godot's body_entered / body_exited). Ids are caller-defined (body indices, entity handles, …).
class AreaMonitor {
public:
    // Diff `current` against the previous frame's membership. Fills entered (in current, not before) and
    // exited (was inside, not now), then adopts `current` as the new membership. Both outputs are sorted.
    void update(const std::vector<int>& current, std::vector<int>& entered, std::vector<int>& exited) {
        std::vector<int> cur = current;
        std::sort(cur.begin(), cur.end());
        cur.erase(std::unique(cur.begin(), cur.end()), cur.end());

        entered.clear();
        exited.clear();
        std::set_difference(cur.begin(), cur.end(), members_.begin(), members_.end(),
                            std::back_inserter(entered));
        std::set_difference(members_.begin(), members_.end(), cur.begin(), cur.end(),
                            std::back_inserter(exited));
        members_ = std::move(cur);
    }

    const std::vector<int>& members() const { return members_; }
    bool contains(int id) const {
        return std::binary_search(members_.begin(), members_.end(), id);
    }
    void clear() { members_.clear(); }

private:
    std::vector<int> members_; // currently inside, kept sorted
};

} // namespace maz::game
