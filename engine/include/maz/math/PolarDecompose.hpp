#pragma once

#include "maz/math/Math.hpp" // mat3, glm transpose/inverse/determinant

#include <cmath>

// maz::math::polarDecompose — split a 3x3 transform into a pure ROTATION times a symmetric STRETCH:
// M = R * S, where R is a proper rotation (orthonormal, det +1) and S is symmetric (the scale/shear part).
// This is the "extract the rotation" operation animation and simulation keep needing: a matrix that has
// accumulated non-uniform scale, shear, or numerical drift (blended skinning matrices, interpolated bone
// transforms, a deformed element) can be cleaned back to its nearest rotation. It is the heart of
// co-rotational / shape-matching deformation (Muller et al.), of orthonormalizing a drifted basis, and of
// recovering a stable orientation from a squished transform. Computed by Higham's quadratically-convergent
// iteration R_{k+1} = 1/2 (gamma R_k + gamma^-1 (R_k^-T)), which drives R to the orthogonal polar factor;
// S = R^T M is then symmetrized. Godot's Basis has orthonormalize() but no true polar decomposition (its
// Gram-Schmidt depends on axis order and does not give the closest rotation). Header-only, glm-backed,
// deterministic. Assumes det(M) > 0 (a right-handed transform); returns false otherwise.
namespace maz::math {

namespace detail {

inline float frobeniusNorm(const mat3& m) {
    float s = 0.0f;
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            s += m[c][r] * m[c][r];
        }
    }
    return std::sqrt(s);
}

} // namespace detail

// Decompose M into rotation R (orthonormal, det +1) and symmetric S with M = R*S. Returns false if M is
// (near) singular or left-handed (det <= 0), in which case R/S are left as identity/M.
inline bool polarDecompose(const mat3& m, mat3& rotation, mat3& stretch) {
    if (glm::determinant(m) <= 1e-8f) {
        rotation = mat3(1.0f);
        stretch = m;
        return false;
    }
    mat3 r = m;
    for (int iter = 0; iter < 64; ++iter) {
        const mat3 rInvT = glm::transpose(glm::inverse(r));
        // Frobenius-norm scaling factor accelerates and stabilizes convergence for scaled inputs.
        const float na = detail::frobeniusNorm(r);
        const float nb = detail::frobeniusNorm(rInvT);
        const float gamma = (na > 1e-20f && nb > 1e-20f) ? std::sqrt(nb / na) : 1.0f;
        const float ig = 1.0f / gamma;
        mat3 next(0.0f);
        for (int c = 0; c < 3; ++c) {
            for (int rr = 0; rr < 3; ++rr) {
                next[c][rr] = 0.5f * (gamma * r[c][rr] + ig * rInvT[c][rr]);
            }
        }
        const mat3 diff = next - r;
        r = next;
        if (detail::frobeniusNorm(diff) < 1e-7f) {
            break;
        }
    }
    // S = R^T M, then symmetrize to remove tiny asymmetry from finite iteration.
    mat3 s = glm::transpose(r) * m;
    mat3 sym(0.0f);
    for (int c = 0; c < 3; ++c) {
        for (int rr = 0; rr < 3; ++rr) {
            sym[c][rr] = 0.5f * (s[c][rr] + s[rr][c]);
        }
    }
    rotation = r;
    stretch = sym;
    return true;
}

// Convenience: the nearest proper rotation to M (the R factor). Returns identity if M is singular/left-handed.
inline mat3 extractRotation(const mat3& m) {
    mat3 r, s;
    polarDecompose(m, r, s);
    return r;
}

} // namespace maz::math
