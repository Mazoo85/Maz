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

    // Project every point onto each axis to find the box extents.
    const vec3 centroid(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    float lo[3] = {1e30f, 1e30f, 1e30f};
    float hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const vec3& p : points) {
        const vec3 d = p - centroid;
        for (int i = 0; i < 3; ++i) {
            const float t = glm::dot(d, axis[i]);
            lo[i] = std::min(lo[i], t);
            hi[i] = std::max(hi[i], t);
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
