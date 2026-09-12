#pragma once

#include "maz/core/Fixed.hpp"
#include "maz/math/FixedVec3.hpp"
#include "maz/math/FixedTrig.hpp" // fixSinCos for from-axis-angle

// maz::math::FixedQuat — a unit quaternion of core::Fixed (Q16.16) for DETERMINISTIC 3D rotation, the
// deterministic companion to the float Quaternion. 3D orientation can't be represented deterministically
// without it: this is what lets a lockstep-multiplayer or replay-exact 3D game turn, aim and spin things
// with bit-identical results on every machine. Built on FixedVec3 (M421) and the integer-CORDIC
// FixedTrig (M417), so the whole from-axis-angle → compose → rotate-a-vector path is pure integer math.
// Hamilton product, conjugate, normalize, and the fast vector-rotation formula (v + 2w(u×v) + 2u×(u×v)).
// Fixed-point precision means rotations are accurate to ~1e-2, plenty for gameplay. Header-only. Godot
// has no fixed-point quaternion. The rotation capstone of the deterministic-sim toolkit (M415-M422).
namespace maz::math {

struct FixedQuat {
    Fixed x;
    Fixed y;
    Fixed z;
    Fixed w;

    constexpr FixedQuat() = default;
    constexpr FixedQuat(Fixed x_, Fixed y_, Fixed z_, Fixed w_) : x(x_), y(y_), z(z_), w(w_) {}

    static constexpr FixedQuat identity() {
        return FixedQuat(Fixed::zero(), Fixed::zero(), Fixed::zero(), Fixed::one());
    }

    // Rotation of `angle` radians about a (non-zero) axis; the axis is normalized internally.
    static FixedQuat fromAxisAngle(FixedVec3 axis, Fixed angle) {
        const FixedVec3 n = axis.normalized();
        if (n == FixedVec3::zero()) {
            return identity();
        }
        const FixSinCos sc = fixSinCos(angle * Fixed::half());
        return FixedQuat(n.x * sc.sin, n.y * sc.sin, n.z * sc.sin, sc.cos);
    }

    // The vector (imaginary) part as a FixedVec3.
    FixedVec3 vec() const { return FixedVec3(x, y, z); }

    // Hamilton product: applying (*this then o) as rotations composes right-to-left like matrices.
    FixedQuat operator*(FixedQuat o) const {
        return FixedQuat(w * o.x + x * o.w + y * o.z - z * o.y,
                         w * o.y - x * o.z + y * o.w + z * o.x,
                         w * o.z + x * o.y - y * o.x + z * o.w,
                         w * o.w - x * o.x - y * o.y - z * o.z);
    }

    constexpr FixedQuat conjugate() const { return FixedQuat(-x, -y, -z, w); }

    Fixed dot(FixedQuat o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    Fixed lengthSquared() const { return x * x + y * y + z * z + w * w; }
    Fixed length() const { return lengthSquared().sqrt(); }

    // Best-effort unit quaternion; returns identity for a (near-)zero quaternion.
    FixedQuat normalized() const {
        const Fixed len = length();
        if (len == Fixed::zero()) {
            return identity();
        }
        return FixedQuat(x / len, y / len, z / len, w / len);
    }

    constexpr bool operator==(FixedQuat o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    constexpr bool operator!=(FixedQuat o) const { return !(*this == o); }

    // Rotate a vector by this (assumed unit) quaternion: v + 2w(u×v) + 2u×(u×v), where u is the vec part.
    FixedVec3 rotate(FixedVec3 v) const {
        const FixedVec3 u = vec();
        const Fixed two = Fixed::fromInt(2);
        const FixedVec3 t = u.cross(v) * two;
        return v + t * w + u.cross(t);
    }
};

} // namespace maz::math
