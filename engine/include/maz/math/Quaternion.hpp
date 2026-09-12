#pragma once

#include "maz/math/Math.hpp"      // vec3, mat3, quat, dot, normalize
#include "maz/math/MathFuncs.hpp" // isEqualApproxf

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

    // Shortest-arc rotation taking direction `from` onto direction `to` — Godot's Quaternion(v0, v1).
    // Inputs need not be unit length (they are normalized here). When they are exactly opposite the
    // arc is a 180° turn about an arbitrary perpendicular axis (Godot picks one deterministically).
    static Quaternion fromTo(const vec3& from, const vec3& to) {
        const vec3 v0 = normalize(from);
        const vec3 v1 = normalize(to);
        const vec3 c = cross(v0, v1);
        const float d = glm::dot(v0, v1);
        if (d < -1.0f + 1e-6f) {
            // Antiparallel: rotate 180° about any axis perpendicular to v0.
            vec3 axis = cross(vec3(1.0f, 0.0f, 0.0f), v0);
            if (glm::length(axis) < 1e-6f) {
                axis = cross(vec3(0.0f, 1.0f, 0.0f), v0);
            }
            return Quaternion(glm::angleAxis(3.14159265358979324f, normalize(axis)));
        }
        const float s = std::sqrt((1.0f + d) * 2.0f);
        const float rs = 1.0f / s;
        return Quaternion(c.x * rs, c.y * rs, c.z * rs, s * 0.5f);
    }

    // Orientation that faces `forward` with `up` kept roughly up — the rotation-only companion to
    // Transform3D::lookingAt (same convention: the object's local -Z axis points along `forward`, matching
    // Godot's "forward"). Use this when you want just the target ORIENTATION to slerp toward (turret/enemy/
    // camera aiming) rather than a full transform. `forward` need not be unit length. If `forward` is
    // degenerate (zero) the identity is returned; if `up` is parallel to `forward` an alternate up is chosen.
    static Quaternion lookRotation(const vec3& forward, const vec3& up = vec3(0.0f, 1.0f, 0.0f)) {
        const float flen = glm::length(forward);
        if (flen < 1e-8f) return Quaternion(); // no direction -> identity
        const vec3 vz = -(forward / flen);     // -Z faces `forward`
        vec3 upv = up;
        if (glm::length(cross(upv, vz)) < 1e-6f) {
            // up parallel to the view axis: pick a different up so the basis is well-defined.
            upv = (std::fabs(vz.y) < 0.99f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
        }
        const vec3 vx = normalize(cross(upv, vz));
        const vec3 vy = cross(vz, vx);
        return fromMat3(mat3(vx, vy, vz));
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

    // Rotation axis of this quaternion (unit) — Godot's get_axis. Near identity (|w|~1) there is no
    // meaningful axis; Godot returns the raw (x,y,z), which is ~0, matching here.
    vec3 getAxis() const {
        const quat n = glm::normalize(q);
        if (std::fabs(n.w) > 1.0f - 1e-6f) {
            return vec3(n.x, n.y, n.z);
        }
        const float r = 1.0f / std::sqrt(1.0f - n.w * n.w);
        return vec3(n.x * r, n.y * r, n.z * r);
    }
    // Rotation angle in radians, in [0, 2*pi) — Godot's get_angle (2*acos(w)).
    float getAngle() const {
        const quat n = glm::normalize(q);
        return 2.0f * std::acos(std::fmin(1.0f, std::fmax(-1.0f, n.w)));
    }

    Quaternion normalized() const { return Quaternion(glm::normalize(q)); }
    Quaternion inverse() const { return Quaternion(glm::inverse(q)); }
    float length() const { return glm::length(q); }
    float lengthSquared() const { return glm::dot(q, q); } // Godot's length_squared
    float dot(const Quaternion& o) const { return glm::dot(q, o.q); }

    // Validation predicates — Godot's Quaternion.is_finite / is_equal_approx / is_normalized.
    // Guard interpolation and physics state against NaN/inf orientations, compare orientations up to
    // float rounding, and confirm a quaternion is a valid (unit) rotation before it is used as one.
    bool isFinite() const {
        return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
    }
    bool isEqualApprox(const Quaternion& o) const {
        return isEqualApproxf(q.x, o.q.x) && isEqualApproxf(q.y, o.q.y) &&
               isEqualApproxf(q.z, o.q.z) && isEqualApproxf(q.w, o.q.w);
    }
    // Squared length within Godot's looser absolute UNIT_EPSILON (0.001), matching Godot exactly.
    bool isNormalized() const { return std::abs(lengthSquared() - 1.0f) < 0.001f; }

    // Shortest-arc unsigned angle between the two orientations, in radians — Godot's angle_to.
    float angleTo(const Quaternion& o) const {
        const float d = glm::dot(glm::normalize(q), glm::normalize(o.q));
        return std::acos(std::fmin(1.0f, std::fmax(-1.0f, 2.0f * d * d - 1.0f)));
    }

    // Spherical-linear interpolation to `to` (both should be unit) — Godot's slerp.
    Quaternion slerp(const Quaternion& to, float t) const {
        return Quaternion(glm::normalize(glm::slerp(glm::normalize(q), glm::normalize(to.q), t)));
    }

    // Spherical-linear interpolation WITHOUT the shortest-path flip — Godot's slerpni ("no
    // inversion"). Plain slerp negates `to` when the dot is negative so it always takes the short
    // way round; slerpni skips that, so it can travel the long arc (>180°). Useful when the caller
    // has already chosen a winding and does not want it silently reversed.
    Quaternion slerpni(const Quaternion& to, float t) const {
        const quat from = glm::normalize(q);
        const quat dst = glm::normalize(to.q);
        const float d = glm::dot(from, dst);
        if (std::fabs(d) > 0.9999f) {
            return Quaternion(from); // essentially identical orientations — nothing to interpolate
        }
        const float theta = std::acos(d);
        const float invSin = 1.0f / std::sin(theta);
        const float wFrom = std::sin((1.0f - t) * theta) * invSin;
        const float wTo = std::sin(t * theta) * invSin;
        return Quaternion(glm::normalize(from * wFrom + dst * wTo));
    }

    // Quaternion logarithm — Godot's Quaternion.log. Maps a unit rotation quaternion to the pure
    // (w = 0) quaternion whose vector part is axis * angle (the rotation vector). log/exp are
    // inverses on unit quaternions and are the building blocks of quaternion spline interpolation.
    Quaternion log() const {
        const vec3 v = getAxis() * getAngle();
        return Quaternion(v.x, v.y, v.z, 0.0f);
    }

    // Quaternion exponential — Godot's Quaternion.exp. Inverse of log(): takes a pure quaternion
    // holding a rotation vector (axis * angle in its x,y,z) back to the corresponding unit rotation.
    Quaternion exp() const {
        vec3 v(q.x, q.y, q.z);
        const float theta = glm::length(v);
        if (theta < 1e-6f) {
            return Quaternion(); // ~zero rotation vector -> identity
        }
        v /= theta;
        return Quaternion::fromAxisAngle(v, theta);
    }

    // SQUAD-style cubic quaternion interpolation guided by the neighbouring `preA`/`postB`
    // orientations — Godot's Quaternion.spherical_cubic_interpolate. A faithful port of Godot's
    // algorithm: normalize the four samples, flip `pre`/`to`/`post` to the shortest hemisphere, run a
    // scalar cubic on the log-map coordinates in both the `from` and `to` tangent spaces, then slerp
    // the two Expmap results to cancel the map's ambiguity. Endpoints are exact (weight 0 -> this,
    // weight 1 -> `b`) and the result is always a unit quaternion.
    Quaternion sphericalCubicInterpolate(const Quaternion& b, const Quaternion& preA,
                                         const Quaternion& postB, float weight) const {
        auto neg = [](const Quaternion& x) { return Quaternion(-x.x(), -x.y(), -x.z(), -x.w()); };
        auto cubic = [](float from, float to, float pre, float post, float w) {
            const float w2 = w * w;
            const float w3 = w2 * w;
            return 0.5f * ((from * 2.0f) + (-pre + to) * w +
                           (2.0f * pre - 5.0f * from + 4.0f * to - post) * w2 +
                           (-pre + 3.0f * from - 3.0f * to + post) * w3);
        };

        Quaternion fromQ = normalized();
        Quaternion preQ = preA.normalized();
        Quaternion toQ = b.normalized();
        Quaternion postQ = postB.normalized();

        // Flip to the shortest hemisphere (Godot's exact sign logic).
        preQ = std::signbit(fromQ.dot(preQ)) ? neg(preQ) : preQ;
        const bool flip2 = std::signbit(fromQ.dot(toQ));
        toQ = flip2 ? neg(toQ) : toQ;
        const bool flip3 = flip2 ? (toQ.dot(postQ) <= 0.0f) : std::signbit(toQ.dot(postQ));
        postQ = flip3 ? neg(postQ) : postQ;

        // Expmap in from-space (from's log coordinate is the origin).
        const Quaternion lnTo1 = (fromQ.inverse() * toQ).log();
        const Quaternion lnPre1 = (fromQ.inverse() * preQ).log();
        const Quaternion lnPost1 = (fromQ.inverse() * postQ).log();
        const Quaternion ln1(cubic(0.0f, lnTo1.x(), lnPre1.x(), lnPost1.x(), weight),
                             cubic(0.0f, lnTo1.y(), lnPre1.y(), lnPost1.y(), weight),
                             cubic(0.0f, lnTo1.z(), lnPre1.z(), lnPost1.z(), weight), 0.0f);
        const Quaternion q1 = fromQ * ln1.exp();

        // Expmap in to-space (to's log coordinate is the origin).
        const Quaternion lnFrom2 = (toQ.inverse() * fromQ).log();
        const Quaternion lnPre2 = (toQ.inverse() * preQ).log();
        const Quaternion lnPost2 = (toQ.inverse() * postQ).log();
        const Quaternion ln2(cubic(lnFrom2.x(), 0.0f, lnPre2.x(), lnPost2.x(), weight),
                             cubic(lnFrom2.y(), 0.0f, lnPre2.y(), lnPost2.y(), weight),
                             cubic(lnFrom2.z(), 0.0f, lnPre2.z(), lnPost2.z(), weight), 0.0f);
        const Quaternion q2 = toQ * ln2.exp();

        return q1.slerp(q2, weight);
    }

    // Time-parametrised SQUAD — Godot's Quaternion.spherical_cubic_interpolate_in_time. Identical to
    // sphericalCubicInterpolate but the scalar cubic on the log-map coordinates is the non-uniform
    // (Barry-Goldman) cubicInterpolateInTime, with `b`/`preA`/`postB` sitting at their own times
    // (`bT`, `preAT`, `postBT`). With uniform times (preAT=-1, bT=1, postBT=2) it collapses onto
    // sphericalCubicInterpolate, exactly as cubicInterpolateInTime collapses onto the plain cubic.
    Quaternion sphericalCubicInterpolateInTime(const Quaternion& b, const Quaternion& preA,
                                               const Quaternion& postB, float weight, float bT,
                                               float preAT, float postBT) const {
        auto neg = [](const Quaternion& x) { return Quaternion(-x.x(), -x.y(), -x.z(), -x.w()); };
        auto cubicT = [](float from, float to, float pre, float post, float w, float toT, float preT,
                         float postT) {
            auto lerpT = [](float a, float bb, float t) { return a + (bb - a) * t; };
            const float t = toT * w;
            const float a1 = lerpT(pre, from, preT == 0.0f ? 0.0f : (t - preT) / -preT);
            const float a2 = lerpT(from, to, toT == 0.0f ? 0.5f : t / toT);
            const float a3 = lerpT(to, post, (postT - toT) == 0.0f ? 1.0f : (t - toT) / (postT - toT));
            const float b1 = lerpT(a1, a2, (toT - preT) == 0.0f ? 0.0f : (t - preT) / (toT - preT));
            const float b2 = lerpT(a2, a3, postT == 0.0f ? 1.0f : t / postT);
            return lerpT(b1, b2, toT == 0.0f ? 0.5f : t / toT);
        };

        Quaternion fromQ = normalized();
        Quaternion preQ = preA.normalized();
        Quaternion toQ = b.normalized();
        Quaternion postQ = postB.normalized();

        preQ = std::signbit(fromQ.dot(preQ)) ? neg(preQ) : preQ;
        const bool flip2 = std::signbit(fromQ.dot(toQ));
        toQ = flip2 ? neg(toQ) : toQ;
        const bool flip3 = flip2 ? (toQ.dot(postQ) <= 0.0f) : std::signbit(toQ.dot(postQ));
        postQ = flip3 ? neg(postQ) : postQ;

        const Quaternion lnTo1 = (fromQ.inverse() * toQ).log();
        const Quaternion lnPre1 = (fromQ.inverse() * preQ).log();
        const Quaternion lnPost1 = (fromQ.inverse() * postQ).log();
        const Quaternion ln1(
            cubicT(0.0f, lnTo1.x(), lnPre1.x(), lnPost1.x(), weight, bT, preAT, postBT),
            cubicT(0.0f, lnTo1.y(), lnPre1.y(), lnPost1.y(), weight, bT, preAT, postBT),
            cubicT(0.0f, lnTo1.z(), lnPre1.z(), lnPost1.z(), weight, bT, preAT, postBT), 0.0f);
        const Quaternion q1 = fromQ * ln1.exp();

        const Quaternion lnFrom2 = (toQ.inverse() * fromQ).log();
        const Quaternion lnPre2 = (toQ.inverse() * preQ).log();
        const Quaternion lnPost2 = (toQ.inverse() * postQ).log();
        const Quaternion ln2(
            cubicT(lnFrom2.x(), 0.0f, lnPre2.x(), lnPost2.x(), weight, bT, preAT, postBT),
            cubicT(lnFrom2.y(), 0.0f, lnPre2.y(), lnPost2.y(), weight, bT, preAT, postBT),
            cubicT(lnFrom2.z(), 0.0f, lnPre2.z(), lnPost2.z(), weight, bT, preAT, postBT), 0.0f);
        const Quaternion q2 = toQ * ln2.exp();

        return q1.slerp(q2, weight);
    }

    mat3 toMat3() const { return glm::mat3_cast(glm::normalize(q)); }
    static Quaternion fromMat3(const mat3& m) { return Quaternion(glm::quat_cast(m)); }
};

} // namespace maz::math
