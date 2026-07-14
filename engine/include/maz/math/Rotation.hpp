#pragma once

// Quaternion rotation helpers over glm — the engine-convenient slice of the Godot
// Quaternion/Basis rotation API: fromAxisAngle, fromEuler (documented YXZ order,
// Godot-default), rotate(q,v), angleBetween (unsigned, double-cover-safe), slerp
// (shortest-path), rotateTowards (angle-clamped step), and lookRotation (local -Z
// maps to the given forward, camera/OpenGL/Godot convention). Composes iter5-era
// maz::math (glm quat). Angles are in radians. toEuler / signed-angle /
// swing-twist decomposition are future refinements. Pure functions.

#include "maz/math/Math.hpp"
#include "maz/core/Assert.hpp"

#include <cmath>

#include <glm/gtc/quaternion.hpp> // angleAxis, slerp, quat_cast, mat3_cast (also via Math.hpp)

namespace maz::math {

// Quaternion for a rotation of `radians` about `axis` (right-hand rule). The axis
// is normalized here, so any non-zero length is accepted (glm::angleAxis expects a
// unit axis).
inline quat fromAxisAngle(vec3 axis, float radians) {
    const float len = glm::length(axis);
    MAZ_ASSERT(len > 1e-6f, "fromAxisAngle: zero-length axis");
    return glm::angleAxis(radians, axis / len);
}

// Rotate a vec3 by a quat.
inline vec3 rotate(const quat& q, vec3 v) { return q * v; }

// Compose Euler angles in DOCUMENTED order YXZ: yaw about +Y, then pitch about +X,
// then roll about +Z, matching Godot's default. Order matters.
inline quat fromEuler(vec3 eulerRadians) {
    return glm::angleAxis(eulerRadians.y, vec3(0.0f, 1.0f, 0.0f)) *
           glm::angleAxis(eulerRadians.x, vec3(1.0f, 0.0f, 0.0f)) *
           glm::angleAxis(eulerRadians.z, vec3(0.0f, 0.0f, 1.0f));
}

// Unsigned angle (radians) between two orientations. The abs on the dot handles the
// quaternion double-cover (q and -q are the same rotation); the clamp guards acos's
// domain against floating-point overshoot past 1.
inline float angleBetween(const quat& a, const quat& b) {
    float d = glm::dot(a, b);
    d = d < 0.0f ? -d : d;
    if (d > 1.0f) d = 1.0f;
    return 2.0f * std::acos(d);
}

// Shortest-path slerp. Negating b when the dot is negative takes the short way
// around; glm::slerp otherwise might go the long way. (b is taken by value so the
// negation stays local.)
inline quat slerp(const quat& a, quat b, float t) {
    if (glm::dot(a, b) < 0.0f) b = -b;
    return glm::slerp(a, b, t);
}

// Rotate `from` toward `to` by at most maxRadians (a clamped step). Returns `to`
// (normalized) when already within reach or nearly coincident; otherwise the
// t = maxRadians / angle interpolation covers the partial step.
inline quat rotateTowards(const quat& from, const quat& to, float maxRadians) {
    MAZ_ASSERT(maxRadians >= 0.0f, "rotateTowards: negative maxRadians");
    const float angle = angleBetween(from, to);
    if (angle <= maxRadians || angle < 1e-6f) return glm::normalize(to);
    const float t = maxRadians / angle;
    return slerp(from, to, t);
}

// Build a rotation whose local FORWARD (defined as -Z, camera/OpenGL/Godot
// convention) points along `forward`, with local +Y as close to `up` as possible.
// Constructs an orthonormal basis and quat_casts it.
inline quat lookRotation(vec3 forward, vec3 up = vec3(0.0f, 1.0f, 0.0f)) {
    const vec3 f = glm::normalize(forward); // world dir the local -Z should map to
    vec3 r = glm::cross(f, up);             // right-ish
    const float rl = glm::length(r);
    MAZ_ASSERT(rl > 1e-6f, "lookRotation: forward parallel to up");
    r = r / rl;
    const vec3 u = glm::cross(r, f); // orthonormal up
    // Local axes map: local +X -> r, local +Y -> u, local -Z -> f (so local +Z -> -f).
    // Column-major mat3 columns are the images of local +X, +Y, +Z:
    const mat3 m(r, u, -f);
    return glm::quat_cast(m);
}

} // namespace maz::math
