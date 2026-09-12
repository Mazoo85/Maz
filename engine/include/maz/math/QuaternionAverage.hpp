#pragma once

#include "maz/math/Math.hpp" // quat

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math quaternion averaging — the correct "mean rotation" of a set of orientations. You cannot average
// rotations by averaging their components and re-normalising (that biases toward whichever hemisphere the
// signs happen to land in and breaks entirely for spread-out rotations). The principled answer (Markley et
// al. 2007) is the dominant eigenvector of the 4x4 matrix M = sum w_i q_i q_i^T — which this computes by
// power iteration. Because each term q q^T is identical for q and -q, the result is correctly insensitive to
// quaternion double-cover sign. Uses: blending several bone/IK orientation targets, smoothing a noisy tracked
// orientation over a window, fusing orientation sensors, computing a representative rotation for an LOD or a
// cluster. Godot's Quaternion has slerp but no averaging. Header-only, std-only, deterministic.
namespace maz::math {

namespace qavg_detail {
// Dominant eigenvector of a symmetric 4x4 matrix via cyclic Jacobi rotations (exact and robust even when
// the top two eigenvalues are nearly equal, where power iteration stalls). Writes the eigenvector to `out`.
inline void jacobiDominant4(double A[4][4], double out[4]) {
    double V[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    for (int sweep = 0; sweep < 50; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < 4; ++p) {
            for (int q = p + 1; q < 4; ++q) {
                off += A[p][q] * A[p][q];
            }
        }
        if (off < 1e-30) {
            break;
        }
        for (int p = 0; p < 4; ++p) {
            for (int q = p + 1; q < 4; ++q) {
                if (std::fabs(A[p][q]) < 1e-300) {
                    continue;
                }
                const double phi = 0.5 * std::atan2(2.0 * A[p][q], A[p][p] - A[q][q]);
                const double c = std::cos(phi), s = std::sin(phi);
                for (int k = 0; k < 4; ++k) { // rotate rows
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp + s * akq;
                    A[k][q] = -s * akp + c * akq;
                }
                for (int k = 0; k < 4; ++k) { // rotate columns
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk + s * aqk;
                    A[q][k] = -s * apk + c * aqk;
                }
                for (int k = 0; k < 4; ++k) { // accumulate eigenvectors
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp + s * vkq;
                    V[k][q] = -s * vkp + c * vkq;
                }
            }
        }
    }
    int best = 0;
    for (int k = 1; k < 4; ++k) {
        if (A[k][k] > A[best][best]) {
            best = k;
        }
    }
    for (int i = 0; i < 4; ++i) {
        out[i] = V[i][best];
    }
}
} // namespace qavg_detail

// The weighted average (mean) rotation of `qs`. If `weights` is empty every quaternion counts equally; a
// non-empty `weights` must match `qs` in length. Returns identity for an empty input.
inline quat averageQuaternions(const std::vector<quat>& qs, const std::vector<float>& weights = {}) {
    if (qs.empty()) {
        return quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    // Accumulate M = sum w_i q_i q_i^T (symmetric 4x4), components ordered (x, y, z, w).
    double M[4][4] = {{0.0}};
    for (std::size_t k = 0; k < qs.size(); ++k) {
        const float w = weights.empty() ? 1.0f : weights[k];
        const double c[4] = {static_cast<double>(qs[k].x), static_cast<double>(qs[k].y),
                             static_cast<double>(qs[k].z), static_cast<double>(qs[k].w)};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                M[i][j] += static_cast<double>(w) * c[i] * c[j];
            }
        }
    }
    double v[4];
    qavg_detail::jacobiDominant4(M, v);
    double norm = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    norm = norm > 1e-300 ? 1.0 / norm : 0.0;
    // glm::quat is constructed (w, x, y, z).
    return quat(static_cast<float>(v[3] * norm), static_cast<float>(v[0] * norm),
                static_cast<float>(v[1] * norm), static_cast<float>(v[2] * norm));
}

} // namespace maz::math
