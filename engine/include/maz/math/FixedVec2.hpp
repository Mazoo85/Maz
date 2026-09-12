#pragma once

#include "maz/core/Fixed.hpp"

// maz::math::FixedVec2 — a 2D vector of core::Fixed (Q16.16) for DETERMINISTIC 2D simulation. Where
// vec2 (float) is right for rendering, FixedVec2 is right for the parts that must agree bit-for-bit
// across machines: lockstep-multiplayer movement, replay-exact physics, cross-platform gameplay. All
// arithmetic is the underlying integer math of core::Fixed, so identical inputs give identical bits
// everywhere. Provides the vector staples — add/sub/scale, dot, the scalar 2D cross, squared/true
// length (via the float-free Fixed::sqrt), distance, and a best-effort normalize. Header-only. Godot
// has no fixed-point vector.
namespace maz::math {

using core::Fixed;

struct FixedVec2 {
    Fixed x;
    Fixed y;

    constexpr FixedVec2() = default;
    constexpr FixedVec2(Fixed x_, Fixed y_) : x(x_), y(y_) {}
    static FixedVec2 fromInt(int ix, int iy) { return FixedVec2(Fixed::fromInt(ix), Fixed::fromInt(iy)); }
    static constexpr FixedVec2 zero() { return FixedVec2(Fixed::zero(), Fixed::zero()); }

    constexpr FixedVec2 operator+(FixedVec2 o) const { return FixedVec2(x + o.x, y + o.y); }
    constexpr FixedVec2 operator-(FixedVec2 o) const { return FixedVec2(x - o.x, y - o.y); }
    constexpr FixedVec2 operator-() const { return FixedVec2(-x, -y); }
    FixedVec2 operator*(Fixed s) const { return FixedVec2(x * s, y * s); }
    FixedVec2 operator/(Fixed s) const { return FixedVec2(x / s, y / s); }

    FixedVec2& operator+=(FixedVec2 o) { x += o.x; y += o.y; return *this; }
    FixedVec2& operator-=(FixedVec2 o) { x -= o.x; y -= o.y; return *this; }

    constexpr bool operator==(FixedVec2 o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(FixedVec2 o) const { return !(*this == o); }

    Fixed dot(FixedVec2 o) const { return x * o.x + y * o.y; }
    // The scalar 2D cross product (z of the 3D cross) — signed parallelogram area / turn direction.
    Fixed cross(FixedVec2 o) const { return x * o.y - y * o.x; }

    Fixed lengthSquared() const { return x * x + y * y; }
    Fixed length() const { return lengthSquared().sqrt(); }
    Fixed distanceSquared(FixedVec2 o) const { return (*this - o).lengthSquared(); }
    Fixed distance(FixedVec2 o) const { return (*this - o).length(); }

    // Best-effort unit vector; returns zero for a (near-)zero vector. Fixed-point precision means the
    // result length is approximately 1, not exactly.
    FixedVec2 normalized() const {
        const Fixed len = length();
        if (len == Fixed::zero()) {
            return zero();
        }
        return FixedVec2(x / len, y / len);
    }
};

} // namespace maz::math
