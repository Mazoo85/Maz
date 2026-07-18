#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace maz::math {

// Rect2 — Godot's Rect2. An axis-aligned rectangle given by `position` (the min corner) + `size`, with
// the full set of geometric operations that UI layout, view/camera culling, tilemap regions, and
// broadphase queries all reach for: point and overlap tests, intersection (clip), union (merge),
// containment (encloses), per-side grow/shrink, and expand-to-include-a-point. By convention (matching
// Godot) the right/bottom edges are EXCLUSIVE for point tests, and the operations assume a non-negative
// size — call abs() first if a size may be negative. Header-only, pure math, deterministic.
struct Rect2 {
    vec2 position{0.0f, 0.0f};
    vec2 size{0.0f, 0.0f};

    Rect2() = default;
    Rect2(vec2 pos, vec2 sz) : position(pos), size(sz) {}
    Rect2(float x, float y, float w, float h) : position(x, y), size(w, h) {}

    float left() const { return position.x; }
    float top() const { return position.y; }
    float right() const { return position.x + size.x; }
    float bottom() const { return position.y + size.y; }
    vec2 end() const { return position + size; }              // the max corner
    vec2 center() const { return position + size * 0.5f; }
    float area() const { return size.x * size.y; }
    bool hasArea() const { return size.x > 0.0f && size.y > 0.0f; }

    // Point containment — min-inclusive, max-exclusive (right/bottom edges not included), Godot convention.
    bool hasPoint(vec2 p) const {
        return p.x >= position.x && p.y >= position.y && p.x < position.x + size.x &&
               p.y < position.y + size.y;
    }

    // Overlap test. With includeBorders, edge-touching rectangles count as intersecting.
    bool intersects(const Rect2& b, bool includeBorders = false) const {
        if (includeBorders) {
            if (position.x > b.position.x + b.size.x) return false;
            if (position.x + size.x < b.position.x) return false;
            if (position.y > b.position.y + b.size.y) return false;
            if (position.y + size.y < b.position.y) return false;
        } else {
            if (position.x >= b.position.x + b.size.x) return false;
            if (position.x + size.x <= b.position.x) return false;
            if (position.y >= b.position.y + b.size.y) return false;
            if (position.y + size.y <= b.position.y) return false;
        }
        return true;
    }

    // The overlapping rectangle, or a zero-size rect at this position when they don't overlap.
    Rect2 intersection(const Rect2& b) const {
        if (!intersects(b)) {
            return Rect2(position, vec2(0.0f, 0.0f));
        }
        const float x0 = std::max(position.x, b.position.x);
        const float y0 = std::max(position.y, b.position.y);
        const float x1 = std::min(position.x + size.x, b.position.x + b.size.x);
        const float y1 = std::min(position.y + size.y, b.position.y + b.size.y);
        return Rect2(x0, y0, x1 - x0, y1 - y0);
    }

    // The smallest rectangle containing both.
    Rect2 merge(const Rect2& b) const {
        const float x0 = std::min(position.x, b.position.x);
        const float y0 = std::min(position.y, b.position.y);
        const float x1 = std::max(position.x + size.x, b.position.x + b.size.x);
        const float y1 = std::max(position.y + size.y, b.position.y + b.size.y);
        return Rect2(x0, y0, x1 - x0, y1 - y0);
    }

    // True if `b` lies fully inside this rectangle.
    bool encloses(const Rect2& b) const {
        return b.position.x >= position.x && b.position.y >= position.y &&
               b.position.x + b.size.x <= position.x + size.x &&
               b.position.y + b.size.y <= position.y + size.y;
    }

    // Grow all four sides by `amount` (negative shrinks).
    Rect2 grow(float amount) const {
        return Rect2(position.x - amount, position.y - amount, size.x + amount * 2.0f,
                     size.y + amount * 2.0f);
    }

    // Grow each side independently.
    Rect2 growIndividual(float l, float t, float r, float b) const {
        return Rect2(position.x - l, position.y - t, size.x + l + r, size.y + t + b);
    }

    // Which edge grow_side / grow_individual refer to (Godot's @GlobalScope Side: L,T,R,B = 0..3).
    enum class Side { Left = 0, Top = 1, Right = 2, Bottom = 3 };

    // Grow only the given side by `amount` (negative shrinks) — Godot's Rect2.grow_side.
    Rect2 growSide(Side side, float amount) const {
        return growIndividual(side == Side::Left ? amount : 0.0f, side == Side::Top ? amount : 0.0f,
                              side == Side::Right ? amount : 0.0f,
                              side == Side::Bottom ? amount : 0.0f);
    }

    // Grow the rectangle just enough to include point `p`.
    Rect2 expand(vec2 p) const {
        const float x0 = std::min(position.x, p.x);
        const float y0 = std::min(position.y, p.y);
        const float x1 = std::max(position.x + size.x, p.x);
        const float y1 = std::max(position.y + size.y, p.y);
        return Rect2(x0, y0, x1 - x0, y1 - y0);
    }

    // An equivalent rectangle with a non-negative size (normalizes a rect built with a negative size).
    Rect2 abs() const {
        return Rect2(position.x + std::min(size.x, 0.0f), position.y + std::min(size.y, 0.0f),
                     std::abs(size.x), std::abs(size.y));
    }
};

} // namespace maz::math
