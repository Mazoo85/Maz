#pragma once

#include "maz/core/Fixed.hpp"
#include "maz/math/FixedVec2.hpp"
#include "maz/math/FixedMath.hpp" // fixMin / fixMax / fixClamp

// maz::math::FixedRect2 — Godot's Rect2 as DETERMINISTIC fixed-point: an axis-aligned rectangle in
// core::Fixed coordinates (position = min corner + size), the deterministic companion to the float
// Rect2. Where Rect2 is right for on-screen UI, FixedRect2 is right for the parts of a game that must
// agree bit-for-bit across machines — lockstep-multiplayer broadphase, replay-exact overlap tests,
// deterministic trigger volumes. Same half-open convention as Rect2/Rect2i (left/top inclusive,
// right/bottom exclusive), same API — hasPoint / intersects / intersection / merge / grow / expand /
// abs / clampPoint. All arithmetic is integer fixed-point. Header-only, pure. Godot has no fixed-point
// rectangle.
namespace maz::math {

struct FixedRect2 {
    FixedVec2 position; // min corner
    FixedVec2 size;     // width/height (non-negative for a normalized rect)

    constexpr FixedRect2() = default;
    constexpr FixedRect2(FixedVec2 pos, FixedVec2 sz) : position(pos), size(sz) {}
    static FixedRect2 fromInt(int x, int y, int w, int h) {
        return FixedRect2(FixedVec2::fromInt(x, y), FixedVec2::fromInt(w, h));
    }

    Fixed left() const { return position.x; }
    Fixed top() const { return position.y; }
    Fixed right() const { return position.x + size.x; }
    Fixed bottom() const { return position.y + size.y; }
    FixedVec2 end() const { return position + size; }         // max corner (exclusive)
    FixedVec2 center() const { return position + size * Fixed::half(); }
    Fixed area() const { return size.x * size.y; }

    // Half-open containment: min inclusive, max exclusive (matches Rect2/Rect2i).
    bool hasPoint(FixedVec2 p) const {
        return p.x >= position.x && p.y >= position.y && p.x < right() && p.y < bottom();
    }

    // Do the two rectangles overlap? With touchingCounts, shared edges count as intersecting.
    bool intersects(const FixedRect2& o, bool touchingCounts = false) const {
        if (touchingCounts) {
            return position.x <= o.right() && o.position.x <= right() && position.y <= o.bottom() &&
                   o.position.y <= bottom();
        }
        return position.x < o.right() && o.position.x < right() && position.y < o.bottom() &&
               o.position.y < bottom();
    }

    // The overlapping region (zero-size if they do not overlap).
    FixedRect2 intersection(const FixedRect2& o) const {
        const FixedVec2 mn(fixMax(position.x, o.position.x), fixMax(position.y, o.position.y));
        const FixedVec2 mx(fixMin(right(), o.right()), fixMin(bottom(), o.bottom()));
        if (mx.x <= mn.x || mx.y <= mn.y) {
            return FixedRect2(mn, FixedVec2::zero());
        }
        return FixedRect2(mn, mx - mn);
    }

    // The smallest rectangle containing both.
    FixedRect2 merge(const FixedRect2& o) const {
        const FixedVec2 mn(fixMin(position.x, o.position.x), fixMin(position.y, o.position.y));
        const FixedVec2 mx(fixMax(right(), o.right()), fixMax(bottom(), o.bottom()));
        return FixedRect2(mn, mx - mn);
    }

    // Grow outward on all sides by `by` (negative shrinks) — Godot's Rect2.grow.
    FixedRect2 grow(Fixed by) const {
        return FixedRect2(FixedVec2(position.x - by, position.y - by),
                          FixedVec2(size.x + by * Fixed::fromInt(2), size.y + by * Fixed::fromInt(2)));
    }

    // Enlarge to include a point — Godot's Rect2.expand.
    FixedRect2 expand(FixedVec2 p) const {
        const FixedVec2 mn(fixMin(position.x, p.x), fixMin(position.y, p.y));
        const FixedVec2 mx(fixMax(right(), p.x), fixMax(bottom(), p.y));
        return FixedRect2(mn, mx - mn);
    }

    // Does this rectangle fully contain `o`?
    bool encloses(const FixedRect2& o) const {
        return o.position.x >= position.x && o.position.y >= position.y && o.right() <= right() &&
               o.bottom() <= bottom();
    }

    // Normalize a rect with negative size so position is the true min corner and size non-negative.
    FixedRect2 abs() const {
        const FixedVec2 mn(fixMin(position.x, right()), fixMin(position.y, bottom()));
        return FixedRect2(mn, FixedVec2(size.x.abs(), size.y.abs()));
    }

    // Nearest point inside the rectangle to `p` (clamped to the closed box).
    FixedVec2 clampPoint(FixedVec2 p) const {
        return FixedVec2(fixClamp(p.x, position.x, right()), fixClamp(p.y, position.y, bottom()));
    }

    constexpr bool operator==(const FixedRect2& o) const {
        return position == o.position && size == o.size;
    }
    constexpr bool operator!=(const FixedRect2& o) const { return !(*this == o); }
};

} // namespace maz::math
