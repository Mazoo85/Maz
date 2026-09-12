#pragma once

#include "maz/math/Math.hpp"        // vec3
#include "maz/math/Quaternion.hpp"  // interop with the rotation quaternion type

#include <cmath>
#include <vector>

// maz::math dual quaternions + dual-quaternion linear blending (DQS) — the skinning math that fixes the
// "candy-wrapper" collapse of linear-blend skinning. A unit dual quaternion represents a rigid motion
// (rotation + translation) with no scale/shear: the REAL part is the rotation quaternion, the DUAL part
// encodes the translation (dual = ½·t·real). Its point is BLENDING: averaging several bone transforms
// as dual quaternions and renormalizing yields another *rigid* transform, so a vertex weighted between a
// straight and a twisted bone keeps its volume instead of pinching toward the joint axis (which is
// exactly what linear-blend skinning gets wrong). This complements the engine's Skeleton/skinning
// (linear-blend) path. Pure quaternion algebra over floats — no GPU, no allocation — so it unit-tests
// exactly against hand-computed rigid motions.
namespace maz::math {

namespace detail {
struct Q4 {
    float x, y, z, w;
};
inline Q4 quatMul(const Q4& a, const Q4& b) {
    return Q4{a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
              a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
              a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
              a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Q4 quatConj(const Q4& a) { return Q4{-a.x, -a.y, -a.z, a.w}; }
} // namespace detail

struct DualQuaternion {
    // Real (rotation) part; unit for a valid rigid transform.
    float rx = 0.0f, ry = 0.0f, rz = 0.0f, rw = 1.0f;
    // Dual part (encodes translation).
    float dx = 0.0f, dy = 0.0f, dz = 0.0f, dw = 0.0f;

    static DualQuaternion identity() { return DualQuaternion{}; }

    // Build from a rotation quaternion and a translation. dual = ½ · (t as pure quat) · real.
    static DualQuaternion fromRotationTranslation(const Quaternion& rot, const vec3& t) {
        DualQuaternion dq;
        dq.rx = rot.x();
        dq.ry = rot.y();
        dq.rz = rot.z();
        dq.rw = rot.w();
        const detail::Q4 real{dq.rx, dq.ry, dq.rz, dq.rw};
        const detail::Q4 tp{t.x, t.y, t.z, 0.0f};
        const detail::Q4 d = detail::quatMul(tp, real);
        dq.dx = 0.5f * d.x;
        dq.dy = 0.5f * d.y;
        dq.dz = 0.5f * d.z;
        dq.dw = 0.5f * d.w;
        return dq;
    }
    static DualQuaternion fromTranslation(const vec3& t) {
        return fromRotationTranslation(Quaternion::identity(), t);
    }
    static DualQuaternion fromRotation(const Quaternion& rot) {
        return fromRotationTranslation(rot, vec3(0.0f, 0.0f, 0.0f));
    }

    // The translation this dual quaternion encodes: t = 2 · vec(dual · conj(real)).
    vec3 translation() const {
        const detail::Q4 real{rx, ry, rz, rw};
        const detail::Q4 dual{dx, dy, dz, dw};
        const detail::Q4 t = detail::quatMul(dual, detail::quatConj(real));
        return vec3(2.0f * t.x, 2.0f * t.y, 2.0f * t.z);
    }

    // Apply the rigid motion to a point: rotate by the real part, then translate.
    vec3 transformPoint(const vec3& p) const {
        const detail::Q4 real{rx, ry, rz, rw};
        const detail::Q4 pr =
            detail::quatMul(detail::quatMul(real, detail::Q4{p.x, p.y, p.z, 0.0f}),
                            detail::quatConj(real));
        const vec3 t = translation();
        return vec3(pr.x + t.x, pr.y + t.y, pr.z + t.z);
    }
};

// Dual-quaternion linear blend (DLB): weighted sum of the parts with hemisphere alignment to the first
// entry, then renormalized by the real-part magnitude so the result is again a unit (rigid) transform.
// This is the operation that keeps skinned volume from collapsing. Weights need not sum to 1 (the final
// normalization handles scale); at least one non-degenerate entry is required.
inline DualQuaternion blendDual(const DualQuaternion* dqs, const float* weights, int count) {
    DualQuaternion acc{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    if (count <= 0) {
        return DualQuaternion::identity();
    }
    for (int i = 0; i < count; ++i) {
        float s = weights[i];
        // Align to the first entry's hemisphere so opposite-sign quaternions don't cancel.
        const float dot = dqs[i].rx * dqs[0].rx + dqs[i].ry * dqs[0].ry + dqs[i].rz * dqs[0].rz +
                          dqs[i].rw * dqs[0].rw;
        if (dot < 0.0f) {
            s = -s;
        }
        acc.rx += s * dqs[i].rx;
        acc.ry += s * dqs[i].ry;
        acc.rz += s * dqs[i].rz;
        acc.rw += s * dqs[i].rw;
        acc.dx += s * dqs[i].dx;
        acc.dy += s * dqs[i].dy;
        acc.dz += s * dqs[i].dz;
        acc.dw += s * dqs[i].dw;
    }
    const float mag = std::sqrt(acc.rx * acc.rx + acc.ry * acc.ry + acc.rz * acc.rz + acc.rw * acc.rw);
    if (mag > 1e-12f) {
        const float inv = 1.0f / mag;
        acc.rx *= inv; acc.ry *= inv; acc.rz *= inv; acc.rw *= inv;
        acc.dx *= inv; acc.dy *= inv; acc.dz *= inv; acc.dw *= inv;
    }
    return acc;
}

inline DualQuaternion blendDual(const std::vector<DualQuaternion>& dqs,
                                const std::vector<float>& weights) {
    const int n = static_cast<int>(dqs.size() < weights.size() ? dqs.size() : weights.size());
    return blendDual(dqs.data(), weights.data(), n);
}

} // namespace maz::math
