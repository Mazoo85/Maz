#pragma once

#include "maz/math/FitObb.hpp"   // math::detail::jacobiEigen3 (symmetric 3x3 eigensolve)
#include "maz/math/Math.hpp"     // math::vec3, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render BOUNDING-CYLINDER FIT — the tightest CAPSULE-LIKE cylinder wrapped around a mesh, aligned to the
// object's own long axis rather than a world axis. Where an axis-aligned box (Aabb3) or even an oriented box
// (FitObb) is the natural proxy for a boxy prop, a cylinder is the right hull for anything long-and-round: a
// character's torso or limb, a pillar, a barrel, a thrown log, a rocket. Games use it for capsule colliders,
// trigger volumes, and cheap broad-phase bounds on elongated bodies (Godot's CapsuleShape3D wants exactly a
// radius + height + axis). The method is PCA: centre the vertices, take the covariance matrix's dominant
// eigenvector (via the engine's symmetric Jacobi solver) as the cylinder's length axis, then measure how far the
// points spread ALONG that axis (the height) and AWAY from it (the radius = the farthest perpendicular distance).
// Reuses `math::detail::jacobiEigen3`. Header-only, std-only, deterministic.
//
// Scope note (honest): this is the PCA-aligned fit, not a global minimum-volume optimiser — for a genuinely
// L-shaped or clustered cloud the principal axis may not be the visually obvious one, and the radius is the worst-
// case perpendicular distance (it fully contains every vertex, never clips). It reads only positions. A mesh with
// fewer than two vertices, or one with no spread, returns `valid=false`.
namespace maz::render {

struct BoundingCylinder {
    math::vec3 axis{0, 1, 0};    // unit direction of the cylinder's length
    math::vec3 centre{0, 0, 0};  // midpoint of the cylinder along its axis
    float radius = 0.0f;          // farthest perpendicular distance of any vertex from the axis
    float height = 0.0f;          // extent of the vertices along the axis
    bool valid = false;
};

inline BoundingCylinder fitBoundingCylinder(const shapes::MeshData& mesh) {
    BoundingCylinder out;
    const std::size_t n = mesh.vertices.size();
    if (n < 2) return out;

    // Centroid.
    double cx = 0, cy = 0, cz = 0;
    for (const MeshVertex& v : mesh.vertices) { cx += v.px; cy += v.py; cz += v.pz; }
    const double inv = 1.0 / static_cast<double>(n);
    cx *= inv; cy *= inv; cz *= inv;

    // Covariance matrix (symmetric).
    double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (const MeshVertex& v : mesh.vertices) {
        const double dx = v.px - cx, dy = v.py - cy, dz = v.pz - cz;
        cov[0][0] += dx * dx; cov[0][1] += dx * dy; cov[0][2] += dx * dz;
        cov[1][1] += dy * dy; cov[1][2] += dy * dz; cov[2][2] += dz * dz;
    }
    cov[1][0] = cov[0][1]; cov[2][0] = cov[0][2]; cov[2][1] = cov[1][2];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) cov[i][j] *= inv;

    double evec[3][3], eval[3];
    math::detail::jacobiEigen3(cov, evec, eval);

    // The length axis is the eigenvector of the LARGEST eigenvalue (the direction of most spread).
    int k = 0;
    if (eval[1] > eval[k]) k = 1;
    if (eval[2] > eval[k]) k = 2;
    math::vec3 axis(static_cast<float>(evec[0][k]), static_cast<float>(evec[1][k]), static_cast<float>(evec[2][k]));
    const float alen = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (alen <= 1e-12f) return out; // no spread
    axis = math::vec3(axis.x / alen, axis.y / alen, axis.z / alen);

    const math::vec3 centroid(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));

    // Project onto the axis for the height extent, and measure perpendicular distance for the radius.
    float minT = 0.0f, maxT = 0.0f, maxR2 = 0.0f;
    bool first = true;
    for (const MeshVertex& v : mesh.vertices) {
        const math::vec3 d(v.px - centroid.x, v.py - centroid.y, v.pz - centroid.z);
        const float t = d.x * axis.x + d.y * axis.y + d.z * axis.z;
        const math::vec3 perp(d.x - axis.x * t, d.y - axis.y * t, d.z - axis.z * t);
        const float r2 = perp.x * perp.x + perp.y * perp.y + perp.z * perp.z;
        if (r2 > maxR2) maxR2 = r2;
        if (first) { minT = maxT = t; first = false; }
        else { if (t < minT) minT = t; if (t > maxT) maxT = t; }
    }

    out.axis = axis;
    out.height = maxT - minT;
    out.radius = std::sqrt(maxR2);
    const float mid = (minT + maxT) * 0.5f;
    out.centre = math::vec3(centroid.x + axis.x * mid, centroid.y + axis.y * mid, centroid.z + axis.z * mid);
    out.valid = true;
    return out;
}

} // namespace maz::render
