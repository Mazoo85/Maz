#pragma once

// maz::math oriented-bounding-box fit — the tightest *rotated* box around a cloud of 3D points, found
// by principal component analysis (PCA). An axis-aligned box (Aabb3) hugs the world axes and wastes
// space on a slanted object; an OBB aligns its own axes to the object's dominant directions, giving a
// far tighter proxy for a diagonal mesh, a leaning crate, or a thrown weapon. It is the fit behind
// tight collision proxies, oriented editor gizmos, and better broad-phase bounds than an AABB. The
// method: centre the points, form their 3x3 covariance matrix, take its eigenvectors (via a symmetric
// Jacobi solve) as the box axes, then project the points onto those axes to size the box. Reuses the
// engine's Obb type (Geometry3D). Godot fits neither spheres nor oriented boxes to point sets in
// gameplay code, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic.
//
// Scope note (honest): PCA alone cannot orient a box whose spreads TIE — a cube, a square-section
// beam, a cylinder, anything with rotational symmetry about an axis — because the covariance matrix
// then has a repeated eigenvalue and every direction in that plane is equally an eigenvector. Left at
// that, a cube rotated 30 degrees came back 30 degrees off and 1.87x too big, and merely translating
// it changed the answer. So when two eigenvalues are close this sweeps each pair of axes through a
// quarter turn and keeps the tightest box, measured over every point against leaving the axes alone,
// which means it can never return a worse box than PCA and a well-oriented cloud comes back unchanged.
// A cube now fits exactly (1.0000x) at any offset, as do boxes of every aspect ratio tried.
//
// What that costs: the sweep runs only on the tie, and looks at a bounded sample of a large cloud, so
// it adds a fixed handful of passes rather than scaling — about 47 ms unoptimised on a 41k-point mesh,
// flat from there. The result is still not a guaranteed global minimum-volume box (that needs a hull
// and rotating calipers); it is PCA with the one blind spot filled in.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "maz/math/Geometry3D.hpp"
#include "maz/math/Math.hpp"

namespace maz::math {

namespace detail {

// Diagonalise a symmetric 3x3 matrix with the classic cyclic Jacobi method. On return `v` columns hold
// the orthonormal eigenvectors and `d` the eigenvalues; `a` is destroyed.
inline void jacobiEigen3(double a[3][3], double v[3][3], double d[3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) v[i][j] = (i == j) ? 1.0 : 0.0;

    for (int sweep = 0; sweep < 64; ++sweep) {
        const double off = std::fabs(a[0][1]) + std::fabs(a[0][2]) + std::fabs(a[1][2]);
        if (off < 1e-18) break;
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) < 1e-300) continue;
                const double tau = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double sgn = tau >= 0.0 ? 1.0 : -1.0;
                const double t = sgn / (std::fabs(tau) + std::sqrt(tau * tau + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                const double apq = a[p][q];
                a[p][p] -= t * apq;
                a[q][q] += t * apq;
                a[p][q] = a[q][p] = 0.0;
                for (int r = 0; r < 3; ++r) {
                    if (r != p && r != q) {
                        const double arp = a[r][p], arq = a[r][q];
                        a[r][p] = a[p][r] = c * arp - s * arq;
                        a[r][q] = a[q][r] = s * arp + c * arq;
                    }
                    const double vrp = v[r][p], vrq = v[r][q];
                    v[r][p] = c * vrp - s * vrq;
                    v[r][q] = s * vrp + c * vrq;
                }
            }
        }
    }
    d[0] = a[0][0];
    d[1] = a[1][1];
    d[2] = a[2][2];
}

} // namespace detail

// Fit an oriented bounding box to `points` via PCA. Empty input -> a zero box at the origin; a single
// point -> a zero-size box there. Every input point is enclosed. The axes are orthonormal.
inline Obb fitObb(const std::vector<vec3>& points) {
    Obb box;
    box.half = vec3(0.0f);
    if (points.empty()) return box;
    if (points.size() == 1) {
        box.center = points[0];
        box.half = vec3(0.0f);
        return box;
    }

    // Centroid.
    double cx = 0, cy = 0, cz = 0;
    for (const vec3& p : points) { cx += p.x; cy += p.y; cz += p.z; }
    const double n = static_cast<double>(points.size());
    cx /= n; cy /= n; cz /= n;

    // Covariance matrix (symmetric).
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (const vec3& p : points) {
        const double dx = p.x - cx, dy = p.y - cy, dz = p.z - cz;
        cov[0][0] += dx * dx; cov[0][1] += dx * dy; cov[0][2] += dx * dz;
        cov[1][1] += dy * dy; cov[1][2] += dy * dz; cov[2][2] += dz * dz;
    }
    cov[0][0] /= n; cov[0][1] /= n; cov[0][2] /= n;
    cov[1][1] /= n; cov[1][2] /= n; cov[2][2] /= n;
    cov[1][0] = cov[0][1]; cov[2][0] = cov[0][2]; cov[2][1] = cov[1][2];

    double vec[3][3], val[3];
    detail::jacobiEigen3(cov, vec, val);

    // Extract the three eigenvectors as world-space axes (columns of `vec`).
    vec3 axis[3];
    for (int i = 0; i < 3; ++i) {
        axis[i] = vec3(static_cast<float>(vec[0][i]), static_cast<float>(vec[1][i]), static_cast<float>(vec[2][i]));
        const float len = glm::length(axis[i]);
        axis[i] = (len > 1e-12f) ? axis[i] / len : vec3(i == 0, i == 1, i == 2);
    }

    // Project points onto a candidate frame and report the box it needs. `stride` lets the search
    // below look at a sample of a large cloud; the final answer is always measured over every point.
    const vec3 centroid(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    auto measure = [&points, &centroid](const vec3 frame[3], std::size_t stride, float outLo[3],
                                        float outHi[3]) {
        outLo[0] = outLo[1] = outLo[2] = 1e30f;
        outHi[0] = outHi[1] = outHi[2] = -1e30f;
        for (std::size_t k = 0; k < points.size(); k += stride) {
            const vec3 d = points[k] - centroid;
            for (int i = 0; i < 3; ++i) {
                const float t = glm::dot(d, frame[i]);
                outLo[i] = std::min(outLo[i], t);
                outHi[i] = std::max(outHi[i], t);
            }
        }
        return (outHi[0] - outLo[0]) * (outHi[1] - outLo[1]) * (outHi[2] - outLo[2]);
    };

    float lo[3];
    float hi[3];
    measure(axis, 1, lo, hi);

    // PCA alone cannot orient a box whose spreads TIE. A cube, a square-section beam, a cylinder —
    // anything with rotational symmetry about an axis — gives the covariance matrix a repeated
    // eigenvalue, at which point every direction in that plane is equally an eigenvector and the pair
    // that comes back is decided by rounding. A cube rotated 30 degrees came back 30 degrees off and
    // 1.87x too big, and merely translating it changed the answer.
    //
    // So finish the job the covariance cannot: spin each pair of axes in its own plane and keep the
    // tightest box. A box's extents repeat every quarter turn, so sweeping [0, pi/2) covers every
    // distinct orientation in that plane. Three guards keep this honest and affordable:
    //   * it runs only when two eigenvalues are actually CLOSE, since that is the only case it can
    //     help — well-separated spreads mean PCA already had a clear answer;
    //   * the sweep looks at a bounded SAMPLE of a large cloud, so the cost does not grow without
    //     limit (a box's orientation is decided by its extremes, which a stride still sees);
    //   * whatever angle the sweep likes is then measured against angle zero over EVERY point, and
    //     the smaller of the two wins. So the result is never worse than plain PCA, and a cloud PCA
    //     already oriented correctly comes back unchanged.
    const double spread = std::max(val[0], std::max(val[1], val[2]));
    const bool tied =
        spread <= 0.0 ||
        std::fabs(val[0] - val[1]) < 0.05 * spread || std::fabs(val[0] - val[2]) < 0.05 * spread ||
        std::fabs(val[1] - val[2]) < 0.05 * spread;
    if (tied) {
        const float kQuarterTurn = 1.57079632679489662f;
        const int kCoarse = 24;
        const int kRefine = 12;
        const std::size_t kSampleCap = 2048;
        const std::size_t stride =
            points.size() > kSampleCap ? (points.size() / kSampleCap) : std::size_t{1};

        for (int plane = 0; plane < 3; ++plane) {
            const int i = plane;
            const int j = (plane + 1) % 3;
            auto spin = [&](float t, vec3 out[3]) {
                out[0] = axis[0];
                out[1] = axis[1];
                out[2] = axis[2];
                const float c = std::cos(t);
                const float sn = std::sin(t);
                out[i] = axis[i] * c + axis[j] * sn;
                out[j] = axis[j] * c - axis[i] * sn;
            };

            float sampleLo[3];
            float sampleHi[3];
            float bestSampled = measure(axis, stride, sampleLo, sampleHi);
            float bestAngle = 0.0f;
            float step = kQuarterTurn / static_cast<float>(kCoarse);
            for (int k = 1; k < kCoarse; ++k) {
                const float t = static_cast<float>(k) * step;
                vec3 cand[3];
                spin(t, cand);
                const float v = measure(cand, stride, sampleLo, sampleHi);
                if (v < bestSampled) {
                    bestSampled = v;
                    bestAngle = t;
                }
            }
            for (int r = 0; r < kRefine; ++r) {
                step *= 0.5f;
                for (int side = -1; side <= 1; side += 2) {
                    const float t = bestAngle + static_cast<float>(side) * step;
                    vec3 cand[3];
                    spin(t, cand);
                    const float v = measure(cand, stride, sampleLo, sampleHi);
                    if (v < bestSampled) {
                        bestSampled = v;
                        bestAngle = t;
                    }
                }
            }
            if (bestAngle == 0.0f) {
                continue;
            }
            // Settle it over every point, against leaving the axes alone.
            vec3 rotated[3];
            spin(bestAngle, rotated);
            float rotLo[3];
            float rotHi[3];
            const float rotatedVolume = measure(rotated, 1, rotLo, rotHi);
            float keepLo[3];
            float keepHi[3];
            const float keptVolume = measure(axis, 1, keepLo, keepHi);
            if (rotatedVolume < keptVolume) {
                axis[0] = rotated[0];
                axis[1] = rotated[1];
                axis[2] = rotated[2];
                for (int k = 0; k < 3; ++k) {
                    lo[k] = rotLo[k];
                    hi[k] = rotHi[k];
                }
            } else {
                for (int k = 0; k < 3; ++k) {
                    lo[k] = keepLo[k];
                    hi[k] = keepHi[k];
                }
            }
        }
    }

    box.axes = mat3(axis[0], axis[1], axis[2]); // columns = local axes
    box.half = vec3(0.5f * (hi[0] - lo[0]), 0.5f * (hi[1] - lo[1]), 0.5f * (hi[2] - lo[2]));
    // Box centre = centroid shifted to the middle of the projected extents along each axis.
    box.center = centroid;
    for (int i = 0; i < 3; ++i) box.center += axis[i] * (0.5f * (hi[i] + lo[i]));
    return box;
}

} // namespace maz::math
