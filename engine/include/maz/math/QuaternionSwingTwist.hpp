#pragma once

#include "maz/math/Quaternion.hpp"

// maz::math swing-twist decomposition + nlerp — rotation-math the Quaternion type did not yet have.
// Swing-twist splits any rotation into a "twist" about a chosen axis and a "swing" perpendicular to it,
// so that q == swing * twist. It is the standard tool for joint limits: a shoulder or knuckle can twist
// freely about the bone while its swing (cone) is clamped, and each part is limited independently after
// decomposing. nlerp is normalized linear interpolation — cheaper than slerp and torque-minimal for the
// small-angle blends animation code does every frame (Godot exposes slerp/slerpni but neither
// swing-twist nor nlerp). Header-only, deterministic, reuses the existing Quaternion type.
namespace maz::math {

// Normalized linear interpolation between two orientations. Takes the shortest path (flips `b` when the
// dot is negative), so nlerp(a,b,0)==a and nlerp(a,b,1)==b (up to sign) and the result is always unit.
inline Quaternion nlerp(const Quaternion& a, const Quaternion& b, float t) {
    quat qb = b.q;
    if (glm::dot(a.q, qb) < 0.0f) {
        qb = -qb;
    }
    return Quaternion(glm::normalize(a.q * (1.0f - t) + qb * t));
}

struct SwingTwist {
    Quaternion swing; // component perpendicular to the axis (the "cone")
    Quaternion twist; // component about the axis (the roll)
};

// Decompose `rot` into a twist about `axis` and a perpendicular swing, such that swing * twist == rot.
// The twist is the part of `rot` whose rotation axis is parallel to `axis`; the swing carries the rest.
// When `rot` is a 180-degree rotation perpendicular to `axis` the twist is undefined and returns
// identity (all the rotation is swing).
inline SwingTwist swingTwist(const Quaternion& rot, const vec3& axis) {
    const vec3 a = normalize(axis);
    const vec3 qv(rot.x(), rot.y(), rot.z());
    const vec3 p = a * glm::dot(qv, a); // project the vector part onto the axis
    Quaternion twist(p.x, p.y, p.z, rot.w());
    if (twist.lengthSquared() < 1e-12f) {
        twist = Quaternion::identity(); // singularity: no well-defined twist
    } else {
        twist = twist.normalized();
    }
    const Quaternion swing = (rot * twist.inverse()).normalized();
    return SwingTwist{swing, twist};
}

} // namespace maz::math
