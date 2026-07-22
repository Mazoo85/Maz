#pragma once

#include "maz/math/Quaternion.hpp" // Quaternion (slerp, operator*, inverse)

#include <cmath>
#include <vector>

// maz::math SQUAD — Spherical-and-QUADrangle quaternion interpolation (Shoemake, 1987).
//
// Quaternion::slerp already blends BETWEEN TWO orientations along the shortest arc — the rotation analog
// of a straight line. But playing a sequence of orientation keyframes with slerp gives a path that is
// only C0: it snaps direction at every keyframe (angular velocity jumps), so a camera or bone visibly
// "ticks" as it passes each key. SQUAD is the rotation analog of a cubic spline: it threads a SMOOTH
// (C1-continuous) curve through a list of orientation keyframes, so angular velocity is continuous and
// the motion glides. This is what cinematic camera rigs and skeletal animation use for rotation tracks.
//
// The construction needs the quaternion exponential map: quatLog turns a unit quaternion into its
// tangent (a pure quaternion), quatExp turns a tangent back into a rotation. From those, each keyframe
// gets an "inner" control quaternion s_i = q_i * exp(-(log(q_i^-1 q_{i-1}) + log(q_i^-1 q_{i+1}))/4),
// and a segment is squad(q0,q1,s0,s1,t) = slerp(slerp(q0,q1,t), slerp(s0,s1,t), 2t(1-t)). Adjacent
// segments share the boundary control, which is exactly what makes the join C1. Endpoints are hit
// exactly (t=0 -> q0, t=1 -> q1) regardless of the controls. Header-only, deterministic — unit-tested
// for exact endpoints, unit length, the degenerate all-equal case, and finite-difference C1 continuity.
namespace maz::math {

// Natural log of a UNIT quaternion -> a pure (w=0) quaternion holding the rotation's tangent.
inline Quaternion quatLog(const Quaternion& a) {
    const quat n = glm::normalize(a.q);
    const float vlen = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (vlen < 1e-8f) {
        return Quaternion(0.0f, 0.0f, 0.0f, 0.0f); // log(identity) = 0
    }
    const float s = std::atan2(vlen, n.w) / vlen; // atan2(|v|,w) = half-angle for a unit quat
    return Quaternion(n.x * s, n.y * s, n.z * s, 0.0f);
}

// Exponential of a PURE (w~0) quaternion -> a unit rotation quaternion (inverse of quatLog).
inline Quaternion quatExp(const Quaternion& a) {
    const float vlen = std::sqrt(a.q.x * a.q.x + a.q.y * a.q.y + a.q.z * a.q.z);
    const float w = std::cos(vlen);
    if (vlen < 1e-8f) {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); // exp(0) = identity
    }
    const float s = std::sin(vlen) / vlen;
    return Quaternion(a.q.x * s, a.q.y * s, a.q.z * s, w);
}

// The inner control ("tangent") quaternion at keyframe `curr` given its neighbours. Neighbours are
// flipped onto curr's hemisphere first so the tangent follows the short way round.
inline Quaternion squadIntermediate(const Quaternion& prev, const Quaternion& curr,
                                    const Quaternion& next) {
    const quat qi = glm::normalize(curr.q);
    const quat inv = glm::inverse(qi);
    quat qp = glm::normalize(prev.q);
    quat qn = glm::normalize(next.q);
    if (glm::dot(qi, qp) < 0.0f) qp = -qp;
    if (glm::dot(qi, qn) < 0.0f) qn = -qn;
    const Quaternion lp = quatLog(Quaternion(inv * qp)); // log(q_i^-1 q_{i-1})
    const Quaternion ln = quatLog(Quaternion(inv * qn)); // log(q_i^-1 q_{i+1})
    const quat tangent = -0.25f * (lp.q + ln.q);         // pure quaternion
    const Quaternion e = quatExp(Quaternion(tangent.x, tangent.y, tangent.z, tangent.w));
    return Quaternion(qi * e.q);
}

// One SQUAD segment between orientations q0 and q1 with inner controls s0 (for q0) and s1 (for q1).
// squad(q0,q1,s0,s1,t) = slerp( slerp(q0,q1,t), slerp(s0,s1,t), 2t(1-t) ). Hits q0 at t=0, q1 at t=1.
inline Quaternion squad(const Quaternion& q0, const Quaternion& q1, const Quaternion& s0,
                        const Quaternion& s1, float t) {
    const Quaternion a = q0.slerp(q1, t);
    const Quaternion b = s0.slerp(s1, t);
    return a.slerp(b, 2.0f * t * (1.0f - t));
}

// Convenience: evaluate the SQUAD segment on [q0,q1] given the outer neighbours prev (before q0) and
// next (after q1), computing both inner controls. Adjacent calls that share q0/q1 share the boundary
// control, so a chain built this way is C1 across every join.
inline Quaternion squadSegment(const Quaternion& prev, const Quaternion& q0, const Quaternion& q1,
                               const Quaternion& next, float t) {
    const Quaternion s0 = squadIntermediate(prev, q0, q1);
    const Quaternion s1 = squadIntermediate(q0, q1, next);
    return squad(q0, q1, s0, s1, t);
}

} // namespace maz::math
