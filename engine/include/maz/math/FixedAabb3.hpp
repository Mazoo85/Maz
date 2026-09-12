#pragma once

#include "maz/core/Fixed.hpp"
#include "maz/math/FixedVec3.hpp"
#include "maz/math/FixedMath.hpp" // fixMin / fixMax / fixClamp

// maz::math::FixedAabb3 — Godot's AABB as DETERMINISTIC fixed-point: an axis-aligned box in core::Fixed
// 3D coordinates (position = min corner + size), the 3D sibling of FixedRect2 (M420) and the
// deterministic companion to the float AABB. Where the float AABB is right for rendering/culling,
// FixedAabb3 is right for the parts of a 3D game that must agree bit-for-bit across machines:
// lockstep-multiplayer broadphase, replay-exact overlap tests, deterministic 3D trigger volumes. Same
// half-open convention as FixedRect2/Rect2i (min inclusive, max exclusive), same API — hasPoint /
// intersects / intersection / merge / grow / expand / encloses / abs / clampPoint / center / volume.
// All arithmetic is integer fixed-point. Header-only, pure. Godot has no fixed-point box.
namespace maz::math {

struct FixedAabb3 {
    FixedVec3 position; // min corner
    FixedVec3 size;     // extent along x/y/z (non-negative for a normalized box)

    constexpr FixedAabb3() = default;
    constexpr FixedAabb3(FixedVec3 pos, FixedVec3 sz) : position(pos), size(sz) {}
    static FixedAabb3 fromInt(int x, int y, int z, int w, int h, int d) {
        return FixedAabb3(FixedVec3::fromInt(x, y, z), FixedVec3::fromInt(w, h, d));
    }

    FixedVec3 end() const { return position + size; }             // max corner (exclusive)
    FixedVec3 center() const { return position + size * Fixed::half(); }
    Fixed volume() const { return size.x * size.y * size.z; }

    // Half-open containment: min inclusive, max exclusive (matches FixedRect2/Rect2i).
    bool hasPoint(FixedVec3 p) const {
        const FixedVec3 mx = end();
        return p.x >= position.x && p.y >= position.y && p.z >= position.z && p.x < mx.x &&
               p.y < mx.y && p.z < mx.z;
    }

    // Do the two boxes overlap? With touchingCounts, shared faces count as intersecting.
    bool intersects(const FixedAabb3& o, bool touchingCounts = false) const {
        const FixedVec3 e = end();
        const FixedVec3 oe = o.end();
        if (touchingCounts) {
            return position.x <= oe.x && o.position.x <= e.x && position.y <= oe.y &&
                   o.position.y <= e.y && position.z <= oe.z && o.position.z <= e.z;
        }
        return position.x < oe.x && o.position.x < e.x && position.y < oe.y && o.position.y < e.y &&
               position.z < oe.z && o.position.z < e.z;
    }

    // The overlapping region (zero-size if they do not overlap).
    FixedAabb3 intersection(const FixedAabb3& o) const {
        const FixedVec3 e = end();
        const FixedVec3 oe = o.end();
        const FixedVec3 mn(fixMax(position.x, o.position.x), fixMax(position.y, o.position.y),
                           fixMax(position.z, o.position.z));
        const FixedVec3 mx(fixMin(e.x, oe.x), fixMin(e.y, oe.y), fixMin(e.z, oe.z));
        if (mx.x <= mn.x || mx.y <= mn.y || mx.z <= mn.z) {
            return FixedAabb3(mn, FixedVec3::zero());
        }
        return FixedAabb3(mn, mx - mn);
    }

    // The smallest box containing both.
    FixedAabb3 merge(const FixedAabb3& o) const {
        const FixedVec3 e = end();
        const FixedVec3 oe = o.end();
        const FixedVec3 mn(fixMin(position.x, o.position.x), fixMin(position.y, o.position.y),
                           fixMin(position.z, o.position.z));
        const FixedVec3 mx(fixMax(e.x, oe.x), fixMax(e.y, oe.y), fixMax(e.z, oe.z));
        return FixedAabb3(mn, mx - mn);
    }

    // Grow outward on all sides by `by` (negative shrinks) — Godot's AABB.grow.
    FixedAabb3 grow(Fixed by) const {
        const Fixed two = Fixed::fromInt(2);
        return FixedAabb3(FixedVec3(position.x - by, position.y - by, position.z - by),
                          FixedVec3(size.x + by * two, size.y + by * two, size.z + by * two));
    }

    // Enlarge to include a point — Godot's AABB.expand.
    FixedAabb3 expand(FixedVec3 p) const {
        const FixedVec3 e = end();
        const FixedVec3 mn(fixMin(position.x, p.x), fixMin(position.y, p.y), fixMin(position.z, p.z));
        const FixedVec3 mx(fixMax(e.x, p.x), fixMax(e.y, p.y), fixMax(e.z, p.z));
        return FixedAabb3(mn, mx - mn);
    }

    // Does this box fully contain `o`?
    bool encloses(const FixedAabb3& o) const {
        const FixedVec3 e = end();
        const FixedVec3 oe = o.end();
        return o.position.x >= position.x && o.position.y >= position.y && o.position.z >= position.z &&
               oe.x <= e.x && oe.y <= e.y && oe.z <= e.z;
    }

    // Normalize a box with negative size so position is the true min corner and size non-negative.
    FixedAabb3 abs() const {
        const FixedVec3 e = end();
        const FixedVec3 mn(fixMin(position.x, e.x), fixMin(position.y, e.y), fixMin(position.z, e.z));
        return FixedAabb3(mn, FixedVec3(size.x.abs(), size.y.abs(), size.z.abs()));
    }

    // Nearest point inside the box to `p` (clamped to the closed box).
    FixedVec3 clampPoint(FixedVec3 p) const {
        const FixedVec3 e = end();
        return FixedVec3(fixClamp(p.x, position.x, e.x), fixClamp(p.y, position.y, e.y),
                         fixClamp(p.z, position.z, e.z));
    }

    constexpr bool operator==(const FixedAabb3& o) const {
        return position == o.position && size == o.size;
    }
    constexpr bool operator!=(const FixedAabb3& o) const { return !(*this == o); }
};

} // namespace maz::math
