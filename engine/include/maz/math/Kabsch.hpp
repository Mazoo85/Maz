#pragma once

#include "maz/math/Math.hpp" // vec3, quat (glm)

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math::kabsch — find the rigid transform (rotation + translation, NO scale/shear) that best maps one
// set of 3D points onto another in the least-squares sense. Given N corresponding point pairs (from[i] should
// land near to[i]), it returns the single rotation and translation minimising the summed squared error. This
// is the workhorse behind point-cloud REGISTRATION (line up a scanned/streamed set of points with a
// reference), POSE fitting (recover how a rigid body moved from a few tracked markers), mocap/tracking
// alignment, and procedural retargeting. The engine had per-axis fits and eigen-based OBB fitting but no
// "best rotation between two clouds". This uses Horn's closed-form quaternion solution (1987): build a 4x4
// symmetric matrix from the cross-covariance of the centred clouds, take the eigenvector of its largest
// eigenvalue as the optimal rotation quaternion (via a Jacobi eigensolve), then translation = centroidTo -
// R*centroidFrom. Always yields a PROPER rotation (never a reflection). Header-only, deterministic.
namespace maz::math {

struct RigidTransform {
    quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    vec3 translation{0.0f, 0.0f, 0.0f};

    vec3 apply(const vec3& p) const { return rotation * p + translation; }
};

namespace detail {

// Cyclic Jacobi eigensolve of a symmetric 4x4 matrix. `a` is destroyed; eigenvectors become columns of `v`,
// eigenvalues fill `d`.
inline void jacobi4(double a[4][4], double v[4][4], double d[4]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            v[i][j] = (i == j) ? 1.0 : 0.0;
        }
        d[i] = a[i][i];
    }
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < 3; ++p) {
            for (int q = p + 1; q < 4; ++q) {
                off += std::fabs(a[p][q]);
            }
        }
        if (off < 1e-18) {
            break;
        }
        for (int p = 0; p < 3; ++p) {
            for (int q = p + 1; q < 4; ++q) {
                const double apq = a[p][q];
                if (std::fabs(apq) < 1e-300) {
                    continue;
                }
                const double diff = a[q][q] - a[p][p];
                double t;
                if (std::fabs(diff) < 1e-30) {
                    t = (apq > 0.0 ? 1.0 : -1.0);
                } else {
                    const double theta = diff / (2.0 * apq);
                    t = (theta > 0.0 ? 1.0 : -1.0) /
                        (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                }
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                const double tau = s / (1.0 + c);

                a[p][p] -= t * apq;
                a[q][q] += t * apq;
                a[p][q] = 0.0;
                a[q][p] = 0.0;
                for (int i = 0; i < 4; ++i) {
                    if (i != p && i != q) {
                        const double aip = a[i][p];
                        const double aiq = a[i][q];
                        a[i][p] = aip - s * (aiq + tau * aip);
                        a[p][i] = a[i][p];
                        a[i][q] = aiq + s * (aip - tau * aiq);
                        a[q][i] = a[i][q];
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    const double vip = v[i][p];
                    const double viq = v[i][q];
                    v[i][p] = vip - s * (viq + tau * vip);
                    v[i][q] = viq + s * (vip - tau * viq);
                }
            }
        }
    }
    for (int i = 0; i < 4; ++i) {
        d[i] = a[i][i];
    }
}

} // namespace detail

// Best-fit rigid transform mapping `from[i]` onto `to[i]`. Both must be the same length; with 0 points the
// identity is returned. `ok` (if provided) is set false only for a length mismatch.
inline RigidTransform kabsch(const std::vector<vec3>& from, const std::vector<vec3>& to, bool* ok = nullptr) {
    RigidTransform out;
    if (from.size() != to.size()) {
        if (ok) *ok = false;
        return out;
    }
    if (ok) *ok = true;
    const std::size_t n = from.size();
    if (n == 0) {
        return out;
    }

    // Centroids.
    vec3 cf{0.0f, 0.0f, 0.0f}, ct{0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i < n; ++i) {
        cf += from[i];
        ct += to[i];
    }
    const float inv = 1.0f / static_cast<float>(n);
    cf *= inv;
    ct *= inv;

    // Cross-covariance H[a][b] = sum (from-cf)[a] * (to-ct)[b].
    double H[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (std::size_t i = 0; i < n; ++i) {
        const vec3 p = from[i] - cf;
        const vec3 qv = to[i] - ct;
        const double px = p.x, py = p.y, pz = p.z;
        const double qx = qv.x, qy = qv.y, qz = qv.z;
        H[0][0] += px * qx; H[0][1] += px * qy; H[0][2] += px * qz;
        H[1][0] += py * qx; H[1][1] += py * qy; H[1][2] += py * qz;
        H[2][0] += pz * qx; H[2][1] += pz * qy; H[2][2] += pz * qz;
    }

    // Horn's 4x4 symmetric matrix N from H.
    const double Sxx = H[0][0], Sxy = H[0][1], Sxz = H[0][2];
    const double Syx = H[1][0], Syy = H[1][1], Syz = H[1][2];
    const double Szx = H[2][0], Szy = H[2][1], Szz = H[2][2];
    double N[4][4] = {
        {Sxx + Syy + Szz, Syz - Szy,        Szx - Sxz,        Sxy - Syx},
        {Syz - Szy,       Sxx - Syy - Szz,  Sxy + Syx,        Szx + Sxz},
        {Szx - Sxz,       Sxy + Syx,       -Sxx + Syy - Szz,  Syz + Szy},
        {Sxy - Syx,       Szx + Sxz,        Syz + Szy,       -Sxx - Syy + Szz},
    };

    double V[4][4], d[4];
    detail::jacobi4(N, V, d);

    // Eigenvector of the largest eigenvalue is the optimal rotation quaternion (w,x,y,z).
    int best = 0;
    for (int i = 1; i < 4; ++i) {
        if (d[i] > d[best]) {
            best = i;
        }
    }
    double qw = V[0][best], qx = V[1][best], qy = V[2][best], qz = V[3][best];
    double qn = std::sqrt(qw * qw + qx * qx + qy * qy + qz * qz);
    if (qn < 1e-20) {
        qw = 1.0; qx = qy = qz = 0.0; qn = 1.0;
    }
    qw /= qn; qx /= qn; qy /= qn; qz /= qn;

    out.rotation = quat(static_cast<float>(qw), static_cast<float>(qx),
                        static_cast<float>(qy), static_cast<float>(qz));
    out.translation = ct - out.rotation * cf;
    return out;
}

} // namespace maz::math
