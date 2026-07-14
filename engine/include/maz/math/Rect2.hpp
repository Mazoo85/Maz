#pragma once

// A 2D axis-aligned rectangle stored as position (min corner) + size (width,
// height), the Godot Rect2 analog and 2D counterpart to iter2's 3D Aabb.
//
// Conventions:
//   - contains uses the HALF-OPEN convention: inclusive min edge, exclusive max
//     edge, so a point on the left/top edge is inside but one on the right/bottom
//     edge is not (matching Godot's has_point).
//   - intersects treats edge-touching as NON-intersecting (exclusive borders); a
//     future includeBorders flag is a refinement.
//   - intersection returns the overlap rect (zero-size when disjoint).
//   - merge/expand return the bounding rect over another rect / a point.
//   - grow inflates all four sides (negative shrinks).
//   - Sizes are assumed non-negative. Composes maz::math vec2.

#include "maz/math/Math.hpp"

#include "maz/core/Assert.hpp"

#include <algorithm>

namespace maz::math {

// 2D axis-aligned rectangle: position (min corner / top-left) + size (w, h).
struct Rect2 {
    vec2 position{0.0f, 0.0f};
    vec2 size{0.0f, 0.0f};

    vec2 min() const {
        return position;
    }

    vec2 max() const {
        return position + size;
    }

    vec2 center() const {
        return position + size * 0.5f;
    }

    float area() const {
        return size.x * size.y;
    }

    bool hasArea() const {
        return size.x > 0.0f && size.y > 0.0f;
    }

    // Half-open containment: inclusive min edge, exclusive max edge.
    bool contains(vec2 p) const {
        return p.x >= position.x && p.y >= position.y &&
               p.x < position.x + size.x && p.y < position.y + size.y;
    }

    // AABB overlap with exclusive borders: edge-touching rects return false.
    bool intersects(const Rect2& o) const {
        return position.x < o.position.x + o.size.x &&
               position.x + size.x > o.position.x &&
               position.y < o.position.y + o.size.y &&
               position.y + size.y > o.position.y;
    }

    // The overlapping rectangle, or a zero-size rect if the two are disjoint.
    Rect2 intersection(const Rect2& o) const {
        const float x0 = std::max(position.x, o.position.x);
        const float y0 = std::max(position.y, o.position.y);
        const float x1 = std::min(position.x + size.x, o.position.x + o.size.x);
        const float y1 = std::min(position.y + size.y, o.position.y + o.size.y);
        if (x1 <= x0 || y1 <= y0) {
            return Rect2{};
        }
        return Rect2{vec2(x0, y0), vec2(x1 - x0, y1 - y0)};
    }

    // The smallest rect containing both this and o.
    Rect2 merge(const Rect2& o) const {
        const float x0 = std::min(position.x, o.position.x);
        const float y0 = std::min(position.y, o.position.y);
        const float x1 = std::max(position.x + size.x, o.position.x + o.size.x);
        const float y1 = std::max(position.y + size.y, o.position.y + o.size.y);
        return Rect2{vec2(x0, y0), vec2(x1 - x0, y1 - y0)};
    }

    // Grow the rect to include point p (its bounding box with p).
    Rect2 expand(vec2 p) const {
        const float x0 = std::min(position.x, p.x);
        const float y0 = std::min(position.y, p.y);
        const float x1 = std::max(position.x + size.x, p.x);
        const float y1 = std::max(position.y + size.y, p.y);
        return Rect2{vec2(x0, y0), vec2(x1 - x0, y1 - y0)};
    }

    // Inflate all four sides by by (negative shrinks). A large negative by can
    // produce a negative size — the caller's responsibility, like Godot.
    Rect2 grow(float by) const {
        return Rect2{position - vec2(by, by), size + vec2(2.0f * by, 2.0f * by)};
    }

    static Rect2 fromMinMax(vec2 mn, vec2 mx) {
        return Rect2{mn, mx - mn};
    }
};

} // namespace maz::math
