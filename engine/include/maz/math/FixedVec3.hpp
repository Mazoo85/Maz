#pragma once

#include "maz/core/Fixed.hpp"

// maz::math::FixedVec3 — a 3D vector of core::Fixed (Q16.16) for DETERMINISTIC 3D simulation, the
// deterministic companion to vec3. Where vec3 (float) is right for rendering, FixedVec3 is right for
// the parts of a 3D game that must agree bit-for-bit across machines: lockstep-multiplayer movement,
// replay-exact physics, cross-platform gameplay. All arithmetic is the underlying integer math of
// core::Fixed, so identical inputs give identical bits everywhere. Provides the vector staples —
// add/sub/scale, dot, the true 3D cross product, squared/true length (via the float-free Fixed::sqrt),
// distance, and a best-effort normalize. Header-only. Godot has no fixed-point vector. The 3D sibling
// of M416 FixedVec2.
namespace maz::math {

using core::Fixed;

struct FixedVec3 {
    Fixed x;
    Fixed y;
    Fixed z;

    constexpr FixedVec3() = default;
    constexpr FixedVec3(Fixed x_, Fixed y_, Fixed z_) : x(x_), y(y_), z(z_) {}
    static FixedVec3 fromInt(int ix, int iy, int iz) {
        return FixedVec3(Fixed::fromInt(ix), Fixed::fromInt(iy), Fixed::fromInt(iz));
    }
    static constexpr FixedVec3 zero() { return FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()); }

    constexpr FixedVec3 operator+(FixedVec3 o) const { return FixedVec3(x + o.x, y + o.y, z + o.z); }
    constexpr FixedVec3 operator-(FixedVec3 o) const { return FixedVec3(x - o.x, y - o.y, z - o.z); }
    constexpr FixedVec3 operator-() const { return FixedVec3(-x, -y, -z); }
    FixedVec3 operator*(Fixed s) const { return FixedVec3(x * s, y * s, z * s); }
    FixedVec3 operator/(Fixed s) const { return FixedVec3(x / s, y / s, z / s); }

    FixedVec3& operator+=(FixedVec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    FixedVec3& operator-=(FixedVec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }

    constexpr bool operator==(FixedVec3 o) const { return x == o.x && y == o.y && z == o.z; }
    constexpr bool operator!=(FixedVec3 o) const { return !(*this == o); }

    Fixed dot(FixedVec3 o) const { return x * o.x + y * o.y + z * o.z; }
    // The true 3D cross product (perpendicular to both inputs).
    FixedVec3 cross(FixedVec3 o) const {
        return FixedVec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }

    Fixed lengthSquared() const { return x * x + y * y + z * z; }
    Fixed length() const { return lengthSquared().sqrt(); }
    Fixed distanceSquared(FixedVec3 o) const { return (*this - o).lengthSquared(); }
    Fixed distance(FixedVec3 o) const { return (*this - o).length(); }

    // Best-effort unit vector; returns zero for a (near-)zero vector. Fixed-point precision means the
    // result length is approximately 1, not exactly.
    FixedVec3 normalized() const {
        const Fixed len = length();
        if (len == Fixed::zero()) {
            return zero();
        }
        return FixedVec3(x / len, y / len, z / len);
    }
};

} // namespace maz::math
