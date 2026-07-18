#pragma once

#include "maz/math/Math.hpp" // vec3, mat3, quat, dot, normalize

#include <glm/gtc/quaternion.hpp> // angleAxis, mat3_cast, quat_cast, slerp

#include <cmath>

// maz::math Quaternion — a Godot-style rotation quaternion, the companion to Transform3D. GLM already
// has the raw quat type, but gameplay/animation code wants Godot's Quaternion API: build from an
// axis+angle or from Euler angles, read Euler angles back, rotate a vector (xform), compose with `*`,
// slerp between orientations, and query dot / length / angle_to / inverse. Euler conversion uses
// Godot's exact YXZ convention (from_euler / get_euler) so orientations authored in Godot import
// identically. Header-only, pure, deterministic; convert to/from a 3x3 basis with toMat3 / fromMat3.
//
// Storage is a normalized-on-demand GLM quat (w,x,y,z). Unit-tested by round-tripping Euler angles,
// composing rotations, and comparing xform against explicitly composed Y*X*Z elementary rotations.
namespace maz::math {

struct Quaternion {
    quat q{1.0f, 0.0f, 0.0f, 0.0f}; // identity (w=1)

    Quaternion() = default;
    explicit Quaternion(const quat& raw) : q(raw) {}
    Quaternion(float x, float y, float z, float w) : q(w, x, y, z) {} // Godot's (x,y,z,w) order

    float x() const { return q.x; }
    float y() const { return q.y; }
    float z() const { return q.z; }
    float w() const { return q.w; }

    static Quaternion identity() { return Quaternion(); }

    // Rotation of `radians` about a (normalized) axis — Godot's Quaternion(axis, angle).
    static Quaternion fromAxisAngle(const vec3& axis, float radians) {
        return Quaternion(glm::angleAxis(radians, normalize(axis)));
    }

    // Build from Euler angles using Godot's YXZ order (R = Y(y)*X(x)*Z(z)) — Godot's from_euler.
    static Quaternion fromEuler(const vec3& e) {
        const float hy = e.y * 0.5f, hx = e.x * 0.5f, hz = e.z * 0.5f;
        const float sy = std::sin(hy), cy = std::cos(hy);
        const float sx = std::sin(hx), cx = std::cos(hx);
        const float sz = std::sin(hz), cz = std::cos(hz);
        return Quaternion(sy * cx * sz + cy * sx * cz,  // x
                          sy * cx * cz - cy * sx * sz,  // y
                          -sy * sx * cz + cy * cx * sz, // z
                          sy * sx * sz + cy * cx * cz); // w
    }

    // Recover Euler angles in Godot's YXZ order (inverse of fromEuler) — Godot's get_euler.
    vec3 getEuler() const {
        const mat3 m = glm::mat3_cast(glm::normalize(q)); // m[col][row]
        const float m12 = m[2][1];                        // Godot rows[1][2]
        vec3 e(0.0f);
        const float kHalfPi = 1.57079632679f;
        if (m12 < 1.0f - 1e-6f) {
            if (m12 > -(1.0f - 1e-6f)) {
                e.x = std::asin(-m12);
                e.y = std::atan2(m[2][0], m[2][2]); // rows[0][2], rows[2][2]
                e.z = std::atan2(m[0][1], m[1][1]); // rows[1][0], rows[1][1]
            } else {                                // m12 == -1
                e.x = kHalfPi;
                e.y = std::atan2(m[1][0], m[0][0]); // rows[0][1], rows[0][0]
                e.z = 0.0f;
            }
        } else { // m12 == 1
            e.x = -kHalfPi;
            e.y = -std::atan2(m[1][0], m[0][0]);
            e.z = 0.0f;
        }
        return e;
    }

    // Compose rotations: (a * b) applies b first, then a — Godot's Quaternion operator*.
    Quaternion operator*(const Quaternion& o) const { return Quaternion(q * o.q); }
    // Rotate a vector by this quaternion — Godot's `quat * Vector3` / xform.
    vec3 xform(const vec3& v) const { return q * v; }

    Quaternion normalized() const { return Quaternion(glm::normalize(q)); }
    Quaternion inverse() const { return Quaternion(glm::inverse(q)); }
    float length() const { return glm::length(q); }
    float dot(const Quaternion& o) const { return glm::dot(q, o.q); }

    // Shortest-arc unsigned angle between the two orientations, in radians — Godot's angle_to.
    float angleTo(const Quaternion& o) const {
        const float d = glm::dot(glm::normalize(q), glm::normalize(o.q));
        return std::acos(std::fmin(1.0f, std::fmax(-1.0f, 2.0f * d * d - 1.0f)));
    }

    // Spherical-linear interpolation to `to` (both should be unit) — Godot's slerp.
    Quaternion slerp(const Quaternion& to, float t) const {
        return Quaternion(glm::normalize(glm::slerp(glm::normalize(q), glm::normalize(to.q), t)));
    }

    mat3 toMat3() const { return glm::mat3_cast(glm::normalize(q)); }
    static Quaternion fromMat3(const mat3& m) { return Quaternion(glm::quat_cast(m)); }
};

} // namespace maz::math
