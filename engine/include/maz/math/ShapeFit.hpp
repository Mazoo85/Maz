#pragma once

#include "maz/math/FitObb.hpp"      // detail::jacobiEigen3 (symmetric 3x3 eigensolver)
#include "maz/math/LinearSolve.hpp" // solveLinearSystem
#include "maz/math/Math.hpp"        // vec2, vec3

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math geometric primitive fitting — find the CIRCLE (2D) or SPHERE (3D) that best passes through a
// cloud of measured points. Where the engine's LeastSquares fits a value as a function of x (a line or a
// polynomial), this fits a round SHAPE to scattered positions: recover the centre and radius of an arc from
// a few sampled points (gears, dials, turning circles, curved track segments), fit a bounding sphere to a
// vertex cloud, estimate a planet/orbit radius, or calibrate a circular sensor sweep. It uses the algebraic
// (Kasa) least-squares form, which linearises the fit so it reduces to a tiny normal-equations solve on the
// new dense linear solver — exact on clean data, stable and fast on noisy data. Godot exposes no such fit.
// Header-only, std-only, deterministic.
namespace maz::math {

struct CircleFit {
    vec2 center{0.0f, 0.0f};
    float radius = 0.0f;
    bool ok = false;
};

struct SphereFit {
    vec3 center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    bool ok = false;
};

struct PlaneFit {
    vec3 normal{0.0f, 0.0f, 1.0f}; // unit normal (direction of least spread)
    vec3 centroid{0.0f, 0.0f, 0.0f};
    float d = 0.0f; // plane is normal.x*x + normal.y*y + normal.z*z + d = 0
    bool ok = false;
};

// Best-fit plane through 3D points by TOTAL least squares (minimises orthogonal distance): the plane
// through the centroid whose normal is the smallest-eigenvalue eigenvector of the point covariance. Needs
// >= 3 non-collinear points.
inline PlaneFit fitPlane(const std::vector<vec3>& pts) {
    PlaneFit f;
    if (pts.size() < 3) {
        return f;
    }
    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (const vec3& p : pts) {
        cx += p.x;
        cy += p.y;
        cz += p.z;
    }
    const double n = static_cast<double>(pts.size());
    cx /= n;
    cy /= n;
    cz /= n;
    double a[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (const vec3& p : pts) {
        const double dx = p.x - cx, dy = p.y - cy, dz = p.z - cz;
        a[0][0] += dx * dx;
        a[0][1] += dx * dy;
        a[0][2] += dx * dz;
        a[1][1] += dy * dy;
        a[1][2] += dy * dz;
        a[2][2] += dz * dz;
    }
    a[1][0] = a[0][1];
    a[2][0] = a[0][2];
    a[2][1] = a[1][2];
    double v[3][3], d[3];
    detail::jacobiEigen3(a, v, d);
    // Smallest eigenvalue (covariance is PSD) -> direction of least spread -> plane normal.
    int mi = 0;
    if (d[1] < d[mi]) {
        mi = 1;
    }
    if (d[2] < d[mi]) {
        mi = 2;
    }
    vec3 nrm(static_cast<float>(v[0][mi]), static_cast<float>(v[1][mi]), static_cast<float>(v[2][mi]));
    const float ln = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
    if (ln < 1e-9f) {
        return f;
    }
    nrm = vec3(nrm.x / ln, nrm.y / ln, nrm.z / ln);
    f.normal = nrm;
    f.centroid = vec3(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    f.d = -(nrm.x * f.centroid.x + nrm.y * f.centroid.y + nrm.z * f.centroid.z);
    f.ok = true;
    return f;
}

// Best-fit circle through 2D points (Kasa algebraic least squares). Needs >= 3 non-collinear points.
inline CircleFit fitCircle(const std::vector<vec2>& pts) {
    CircleFit f;
    if (pts.size() < 3) {
        return f;
    }
    std::vector<double> M(9, 0.0), rhs(3, 0.0);
    for (const vec2& p : pts) {
        const double row[3] = {2.0 * p.x, 2.0 * p.y, 1.0};
        const double z = static_cast<double>(p.x) * p.x + static_cast<double>(p.y) * p.y;
        for (int i = 0; i < 3; ++i) {
            rhs[static_cast<std::size_t>(i)] += row[i] * z;
            for (int j = 0; j < 3; ++j) {
                M[static_cast<std::size_t>(i * 3 + j)] += row[i] * row[j];
            }
        }
    }
    std::vector<double> x;
    if (!solveLinearSystem(M, rhs, 3, x)) {
        return f; // collinear / degenerate
    }
    const double cx = x[0], cy = x[1], c = x[2];
    const double r2 = c + cx * cx + cy * cy;
    if (r2 < 0.0) {
        return f;
    }
    f.center = vec2(static_cast<float>(cx), static_cast<float>(cy));
    f.radius = static_cast<float>(std::sqrt(r2));
    f.ok = true;
    return f;
}

// Best-fit sphere through 3D points (Kasa algebraic least squares). Needs >= 4 non-coplanar points.
inline SphereFit fitSphere(const std::vector<vec3>& pts) {
    SphereFit f;
    if (pts.size() < 4) {
        return f;
    }
    std::vector<double> M(16, 0.0), rhs(4, 0.0);
    for (const vec3& p : pts) {
        const double row[4] = {2.0 * p.x, 2.0 * p.y, 2.0 * p.z, 1.0};
        const double z = static_cast<double>(p.x) * p.x + static_cast<double>(p.y) * p.y +
                         static_cast<double>(p.z) * p.z;
        for (int i = 0; i < 4; ++i) {
            rhs[static_cast<std::size_t>(i)] += row[i] * z;
            for (int j = 0; j < 4; ++j) {
                M[static_cast<std::size_t>(i * 4 + j)] += row[i] * row[j];
            }
        }
    }
    std::vector<double> x;
    if (!solveLinearSystem(M, rhs, 4, x)) {
        return f; // coplanar / degenerate
    }
    const double cx = x[0], cy = x[1], cz = x[2], d = x[3];
    const double r2 = d + cx * cx + cy * cy + cz * cz;
    if (r2 < 0.0) {
        return f;
    }
    f.center = vec3(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    f.radius = static_cast<float>(std::sqrt(r2));
    f.ok = true;
    return f;
}

} // namespace maz::math
