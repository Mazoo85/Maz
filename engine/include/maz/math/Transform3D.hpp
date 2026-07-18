#pragma once

#include "maz/math/Math.hpp" // mat3, mat4, vec3, quat, cross, dot, normalize

#include <glm/gtc/quaternion.hpp> // angleAxis, quat_cast, mat3_cast, slerp

#include <cmath>

// maz::math Transform3D — the core spatial transform Godot builds every 3D node on: a 3x3 `basis`
// (rotation + scale + shear, columns = the transformed X/Y/Z axes) plus a `vec3 origin`. The engine
// renders with GLM mat4s, but gameplay/tools code wants Godot's ergonomic API — xform / xform_inv,
// affine_inverse, compose with `*`, translated / rotated / scaled (global and _local variants),
// looking_at, and interpolate_with (translation lerp + rotation slerp + scale lerp). This is that
// type, semantics matched to Godot's Transform3D/Basis source. Header-only, pure, deterministic;
// convert to/from the renderer's mat4 with toMat4 / fromMat4. Unit-tested to the bit.
//
// Convention note (matches Godot): looking_at aims the -Z axis at the target (Godot's "forward"),
// and xform_inv / inverse take the fast orthonormal path (transpose) — use affineInverse when the
// basis carries scale or shear.
namespace maz::math {

struct Transform3D {
    mat3 basis{1.0f};  // identity rotation/scale
    vec3 origin{0.0f}; // translation

    Transform3D() = default;
    Transform3D(const mat3& b, const vec3& o) : basis(b), origin(o) {}

    // ---- named constructors ----
    static Transform3D translation(const vec3& t) { return Transform3D(mat3(1.0f), t); }
    static Transform3D rotation(const vec3& axis, float radians) {
        return Transform3D(glm::mat3_cast(glm::angleAxis(radians, normalize(axis))), vec3(0.0f));
    }
    static Transform3D scaling(const vec3& s) {
        return Transform3D(mat3(s.x, 0, 0, 0, s.y, 0, 0, 0, s.z), vec3(0.0f));
    }

    // ---- point / vector transforms ----
    // Full transform of a point (basis then translation) — Godot's Transform3D.xform / `* Vector3`.
    vec3 xform(const vec3& p) const { return basis * p + origin; }
    // Transform a direction (no translation).
    vec3 basisXform(const vec3& v) const { return basis * v; }
    // Inverse transform of a point, assuming an ORTHONORMAL basis — Godot's xform_inv.
    vec3 xformInv(const vec3& p) const { return glm::transpose(basis) * (p - origin); }

    // ---- composition & inverse ----
    // this * other: apply `other` first, then `this` (Godot's Transform3D operator*).
    Transform3D operator*(const Transform3D& o) const {
        return Transform3D(basis * o.basis, basis * o.origin + origin);
    }
    // General inverse (handles scale/shear) — Godot's affine_inverse.
    Transform3D affineInverse() const {
        const mat3 bi = glm::inverse(basis);
        return Transform3D(bi, -(bi * origin));
    }
    // Fast inverse for an orthonormal basis (transpose) — Godot's inverse.
    Transform3D inverse() const {
        const mat3 bt = glm::transpose(basis);
        return Transform3D(bt, -(bt * origin));
    }

    // ---- global vs local moves (mirrors Godot's translated / translated_local etc.) ----
    Transform3D translated(const vec3& ofs) const { return Transform3D(basis, origin + ofs); }
    Transform3D translatedLocal(const vec3& ofs) const {
        return Transform3D(basis, origin + basis * ofs);
    }
    Transform3D rotated(const vec3& axis, float radians) const { return rotation(axis, radians) * *this; }
    Transform3D rotatedLocal(const vec3& axis, float radians) const {
        return *this * rotation(axis, radians);
    }
    // Global scale scales the basis ROWS and the origin component-wise (Godot's scaled).
    Transform3D scaled(const vec3& s) const {
        return Transform3D(mat3(s.x, 0, 0, 0, s.y, 0, 0, 0, s.z) * basis, origin * s);
    }
    // Local scale scales the basis COLUMNS, origin unchanged (Godot's scaled_local).
    Transform3D scaledLocal(const vec3& s) const {
        return Transform3D(basis * mat3(s.x, 0, 0, 0, s.y, 0, 0, 0, s.z), origin);
    }

    // Orthonormalise the basis via Gram-Schmidt (strips scale/shear) — Godot's orthonormalized.
    Transform3D orthonormalized() const {
        vec3 x = basis[0], y = basis[1], z = basis[2];
        x = normalize(x);
        y = normalize(y - x * dot(x, y));
        z = normalize(z - x * dot(x, z) - y * dot(y, z));
        return Transform3D(mat3(x, y, z), origin);
    }

    // Rotate so the -Z axis points at `target` (Godot forward), keeping `up` roughly up — looking_at.
    Transform3D lookingAt(const vec3& target, const vec3& up = vec3(0, 1, 0)) const {
        const vec3 vz = -normalize(target - origin); // -Z faces the target
        vec3 vx = normalize(cross(up, vz));
        const vec3 vy = cross(vz, vx);
        return Transform3D(mat3(vx, vy, vz), origin);
    }

    // Interpolate translation (lerp), rotation (slerp) and scale (lerp) — Godot's interpolate_with.
    Transform3D interpolateWith(const Transform3D& to, float t) const {
        const vec3 sScale = getScale(), dScale = to.getScale();
        const quat sRot = getRotationQuat(), dRot = to.getRotationQuat();
        const quat rot = glm::normalize(glm::slerp(sRot, dRot, t));
        const vec3 scale = sScale * (1.0f - t) + dScale * t;
        mat3 b = glm::mat3_cast(rot);
        b[0] *= scale.x;
        b[1] *= scale.y;
        b[2] *= scale.z;
        return Transform3D(b, origin * (1.0f - t) + to.origin * t);
    }

    // Per-axis scale as the basis column lengths, signed by the determinant (Godot's Basis.get_scale).
    vec3 getScale() const {
        const float sign = glm::determinant(basis) < 0.0f ? -1.0f : 1.0f;
        return sign * vec3(glm::length(basis[0]), glm::length(basis[1]), glm::length(basis[2]));
    }
    // Rotation-only quaternion (basis with scale divided out) — Godot's get_rotation_quaternion.
    quat getRotationQuat() const {
        const vec3 s = getScale();
        mat3 r;
        r[0] = basis[0] / s.x;
        r[1] = basis[1] / s.y;
        r[2] = basis[2] / s.z;
        return glm::quat_cast(r);
    }

    // ---- renderer interop ----
    mat4 toMat4() const {
        mat4 m(basis);
        m[3] = vec4(origin, 1.0f);
        return m;
    }
    static Transform3D fromMat4(const mat4& m) {
        return Transform3D(mat3(m), vec3(m[3]));
    }
};

} // namespace maz::math
