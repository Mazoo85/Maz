#pragma once

#include "maz/math/FitObb.hpp"    // detail::jacobiEigen3 (symmetric 3x3 eigensolver)
#include "maz/math/Math.hpp"      // math::vec3, dot
#include "maz/render/Shapes.hpp"  // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::render DOMINANT-PLANE / FLATNESS DETECTOR — fit the best-matching flat plane to a mesh's vertices and
// measure how FLAT the shape actually is. Via principal component analysis (centre the points, form their 3x3
// covariance, take its eigenvectors), the direction of LEAST spread is the plane's normal and the leftover spread
// along it is how far the shape departs from flat. This answers "is this a wall / floor / panel / decal, and
// which way does it face?" — used to auto-orient flat props to a surface, pick a planar-UV axis, snap a
// billboard, detect ground/wall pieces for gameplay, or decide a nearly-flat mesh can collapse to a quad. Reuses
// the M-era FitObb symmetric-eigen solver. Header-only, pure CPU.
//
// Scope note (honest): this fits ONE global plane through the centroid — great for genuinely planar-ish meshes
// (walls, panels, terrain patches), but a folded or multi-part mesh returns the average best-fit plane, not a
// per-region one (segment first via M529 components / M545 planar regions). `planarity` is a shape descriptor in
// [0,1] (1 = perfectly flat, 0 = isotropic like a cube/ball), not a physical unit; `rmsDistance` and `thickness`
// are in mesh units. Every vertex is weighted equally (not area-weighted), so a dense cluster pulls the fit.
namespace maz::render {

struct MeshPlane {
    bool valid = false;
    math::vec3 normal{0, 0, 0};  // unit normal — the least-spread (flattest) direction
    math::vec3 point{0, 0, 0};   // a point on the plane (the vertex centroid)
    float rmsDistance = 0.0f;    // root-mean-square distance of vertices to the plane (mesh units)
    float thickness = 0.0f;      // max−min vertex distance along the normal (total departure from flat)
    float planarity = 0.0f;      // 0..1 shape flatness: 1 = perfectly flat, 0 = isotropic (cube/ball)
};

// Fit the dominant plane of `mesh` by PCA over its vertices.
inline MeshPlane fitDominantPlane(const shapes::MeshData& mesh) {
    MeshPlane out;
    const std::size_t n = mesh.vertices.size();
    if (n < 3) return out;

    std::vector<math::vec3> p(n);
    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        p[i] = math::vec3(v.px, v.py, v.pz);
        cx += v.px;
        cy += v.py;
        cz += v.pz;
    }
    const double inv = 1.0 / static_cast<double>(n);
    const math::vec3 centroid(static_cast<float>(cx * inv), static_cast<float>(cy * inv),
                              static_cast<float>(cz * inv));

    // Covariance matrix (symmetric, population-normalised so an eigenvalue == variance along that axis).
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (const math::vec3& q : p) {
        const double dx = static_cast<double>(q.x - centroid.x);
        const double dy = static_cast<double>(q.y - centroid.y);
        const double dz = static_cast<double>(q.z - centroid.z);
        cov[0][0] += dx * dx;
        cov[0][1] += dx * dy;
        cov[0][2] += dx * dz;
        cov[1][1] += dy * dy;
        cov[1][2] += dy * dz;
        cov[2][2] += dz * dz;
    }
    cov[0][0] *= inv;
    cov[0][1] *= inv;
    cov[0][2] *= inv;
    cov[1][1] *= inv;
    cov[1][2] *= inv;
    cov[2][2] *= inv;
    cov[1][0] = cov[0][1];
    cov[2][0] = cov[0][2];
    cov[2][1] = cov[1][2];

    double vec[3][3], val[3];
    math::detail::jacobiEigen3(cov, vec, val);

    // Smallest eigenvalue -> least-spread direction -> plane normal.
    int lo = 0;
    if (val[1] < val[lo]) lo = 1;
    if (val[2] < val[lo]) lo = 2;
    const double sum = val[0] + val[1] + val[2];
    math::vec3 normal(static_cast<float>(vec[0][lo]), static_cast<float>(vec[1][lo]),
                      static_cast<float>(vec[2][lo]));
    const float nlen = std::sqrt(math::dot(normal, normal));
    if (nlen <= 1e-20f) return out;
    normal = math::vec3(normal.x / nlen, normal.y / nlen, normal.z / nlen);

    // Thickness = spread of the signed distances to the plane along the normal.
    float dmin = 1e30f, dmax = -1e30f;
    for (const math::vec3& q : p) {
        const float d = math::dot(q - centroid, normal);
        dmin = std::min(dmin, d);
        dmax = std::max(dmax, d);
    }

    out.valid = true;
    out.normal = normal;
    out.point = centroid;
    out.rmsDistance = static_cast<float>(std::sqrt(std::max(0.0, val[lo])));
    out.thickness = dmax - dmin;
    // planarity = 1 - 3*smallest/sum: 1 when the flat direction has ~zero variance, 0 when fully isotropic.
    out.planarity = (sum > 1e-20) ? static_cast<float>(1.0 - 3.0 * val[lo] / sum) : 1.0f;
    out.planarity = std::clamp(out.planarity, 0.0f, 1.0f);
    return out;
}

} // namespace maz::render
