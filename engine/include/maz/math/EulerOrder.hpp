#pragma once

#include "maz/math/Math.hpp" // vec3, mat3

#include <glm/gtc/quaternion.hpp> // angleAxis, mat3_cast

#include <cmath>

// maz::math Euler-order conversions — Godot's Basis.from_euler / get_euler across ALL SIX rotation
// orders (Godot's EulerOrder / Node3D.rotation_order: XYZ, XZY, YXZ, YZX, ZXY, ZYX). The existing
// Quaternion type already covers Godot's default YXZ; this adds the other five so orientations
// authored in Godot with a non-default rotation order import and export identically.
//
// A basis here is a 3x3 rotation matrix (GLM column-major, the same type Transform3D::basis uses).
// from_euler builds R as the ordered product of elementary axis rotations (matching Godot's source:
// e.g. XYZ = Rx*Ry*Rz), where each elementary rotation is the standard right-handed one. get_euler
// inverts that, returning angles in radians as a vec3 (x=rotation about X, y=about Y, z=about Z),
// with a gimbal-lock branch (middle axis at +/-90 deg) that still reconstructs the input basis.
// Header-only, pure, deterministic; unit-tested by round-tripping all six orders (including lock)
// and cross-checking YXZ against Quaternion::fromEuler.
namespace maz::math {

enum class EulerOrder { XYZ, XZY, YXZ, YZX, ZXY, ZYX };

// Euler angles (radians; x about X, y about Y, z about Z) -> rotation basis, in the given order.
inline mat3 basisFromEuler(const vec3& e, EulerOrder order) {
    const mat3 rx = glm::mat3_cast(glm::angleAxis(e.x, vec3(1.0f, 0.0f, 0.0f)));
    const mat3 ry = glm::mat3_cast(glm::angleAxis(e.y, vec3(0.0f, 1.0f, 0.0f)));
    const mat3 rz = glm::mat3_cast(glm::angleAxis(e.z, vec3(0.0f, 0.0f, 1.0f)));
    switch (order) {
    case EulerOrder::XYZ: return rx * ry * rz;
    case EulerOrder::XZY: return rx * rz * ry;
    case EulerOrder::YXZ: return ry * rx * rz;
    case EulerOrder::YZX: return ry * rz * rx;
    case EulerOrder::ZXY: return rz * rx * ry;
    case EulerOrder::ZYX: return rz * ry * rx;
    }
    return mat3(1.0f);
}

// Rotation basis -> Euler angles (radians) in the given order (inverse of basisFromEuler).
inline vec3 basisGetEuler(const mat3& m, EulerOrder order) {
    // E(i,j) reads the STANDARD matrix element R[row=i][col=j]; GLM stores columns, so R[i][j]=m[j][i].
    const auto E = [&](int i, int j) -> float { return m[j][i]; };
    const float kHalfPi = 1.57079632679489662f;
    const float lock = 1.0f - 1e-6f;
    const auto sgn = [](float v) -> float { return v >= 0.0f ? 1.0f : -1.0f; };
    vec3 e(0.0f);
    switch (order) {
    case EulerOrder::XYZ: {
        const float s = E(0, 2);
        if (s < lock && s > -lock) {
            e.y = std::asin(s);
            e.x = std::atan2(-E(1, 2), E(2, 2));
            e.z = std::atan2(-E(0, 1), E(0, 0));
        } else {
            const float sb = sgn(s);
            e.y = sb * kHalfPi;
            e.x = std::atan2(sb * E(1, 0), E(1, 1));
        }
        break;
    }
    case EulerOrder::XZY: {
        const float sc = -E(0, 1);
        if (sc < lock && sc > -lock) {
            e.z = std::asin(sc);
            e.y = std::atan2(E(0, 2), E(0, 0));
            e.x = std::atan2(E(2, 1), E(1, 1));
        } else {
            const float s = sgn(sc);
            e.z = s * kHalfPi;
            e.x = std::atan2(s * E(2, 0), s * E(1, 0));
        }
        break;
    }
    case EulerOrder::YXZ: {
        const float sa = -E(1, 2);
        if (sa < lock && sa > -lock) {
            e.x = std::asin(sa);
            e.z = std::atan2(E(1, 0), E(1, 1));
            e.y = std::atan2(E(0, 2), E(2, 2));
        } else {
            const float s = sgn(sa);
            e.x = s * kHalfPi;
            e.y = std::atan2(s * E(0, 1), E(0, 0));
        }
        break;
    }
    case EulerOrder::YZX: {
        const float sc = E(1, 0);
        if (sc < lock && sc > -lock) {
            e.z = std::asin(sc);
            e.x = std::atan2(-E(1, 2), E(1, 1));
            e.y = std::atan2(-E(2, 0), E(0, 0));
        } else {
            const float s = sgn(sc);
            e.z = s * kHalfPi;
            e.x = std::atan2(s * E(0, 2), -s * E(0, 1));
        }
        break;
    }
    case EulerOrder::ZXY: {
        const float sa = E(2, 1);
        if (sa < lock && sa > -lock) {
            e.x = std::asin(sa);
            e.y = std::atan2(-E(2, 0), E(2, 2));
            e.z = std::atan2(-E(0, 1), E(1, 1));
        } else {
            const float s = sgn(sa);
            e.x = s * kHalfPi;
            e.y = std::atan2(s * E(1, 0), E(0, 0));
        }
        break;
    }
    case EulerOrder::ZYX: {
        const float sb = -E(2, 0);
        if (sb < lock && sb > -lock) {
            e.y = std::asin(sb);
            e.x = std::atan2(E(2, 1), E(2, 2));
            e.z = std::atan2(E(1, 0), E(0, 0));
        } else {
            const float s = sgn(sb);
            e.y = s * kHalfPi;
            e.x = std::atan2(s * E(0, 1), E(1, 1));
        }
        break;
    }
    }
    return e;
}

} // namespace maz::math
