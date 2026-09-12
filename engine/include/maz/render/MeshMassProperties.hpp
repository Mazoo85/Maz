#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render MESH MASS PROPERTIES — compute the VOLUME, CENTER OF MASS, and full INERTIA TENSOR of a solid
// bounded by a closed triangle mesh, assuming uniform density. This is what a physics engine needs to make a
// custom (non-primitive) collider spin correctly: Physics3D already derives inertia for boxes/spheres/
// capsules analytically, but an arbitrary imported hull had no way to get its real mass distribution. Uses
// the signed-tetrahedron method (Mirtich / Blow-Binstock): every triangle forms a tetrahedron with the
// origin, and the signed contributions of those tets sum to the exact integrals over the enclosed solid,
// independent of where the origin sits. Consistent winding is required (the sign is auto-corrected if the
// whole mesh is wound inward). Pure CPU, header-only, headless — verifiable against the closed-form values
// of a unit cube.
//
// Results use double precision and unit density: `mass == volume`. Scale the inertia by real density (or
// desiredMass/volume) to get physical values; shift with the parallel-axis theorem for a non-centroid pivot.
namespace maz::render {

struct MassProperties {
    bool valid = false;          // false when the volume is ~0 (open or degenerate mesh)
    double volume = 0.0;         // = mass at unit density
    double centroid[3] = {0, 0, 0};
    double inertia[3][3] = {};   // about the centroid, unit density (symmetric)
};

inline MassProperties computeMassProperties(const shapes::MeshData& mesh) {
    MassProperties mp;
    if (mesh.indices.size() < 3) return mp;

    double vol6 = 0.0;                 // 6 * volume (sum of tetrahedron determinants)
    double cAccum[3] = {0, 0, 0};      // centroid numerator (later / (4*vol6))
    double C[3][3] = {};               // covariance integral of x x^T over the solid (about origin)

    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const MeshVertex& va = mesh.vertices[mesh.indices[t]];
        const MeshVertex& vb = mesh.vertices[mesh.indices[t + 1]];
        const MeshVertex& vc = mesh.vertices[mesh.indices[t + 2]];
        const double a[3] = {va.px, va.py, va.pz};
        const double b[3] = {vb.px, vb.py, vb.pz};
        const double c[3] = {vc.px, vc.py, vc.pz};
        // det = a . (b x c) = 6 * signed volume of the tetrahedron (origin, a, b, c).
        const double bxc[3] = {b[1] * c[2] - b[2] * c[1],
                               b[2] * c[0] - b[0] * c[2],
                               b[0] * c[1] - b[1] * c[0]};
        const double det = a[0] * bxc[0] + a[1] * bxc[1] + a[2] * bxc[2];

        vol6 += det;
        const double s[3] = {a[0] + b[0] + c[0], a[1] + b[1] + c[1], a[2] + b[2] + c[2]};
        for (int i = 0; i < 3; ++i) cAccum[i] += det * s[i];

        // Covariance of the tetrahedron: det/120 * (a a^T + b b^T + c c^T + s s^T).
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                C[i][j] += det * (a[i] * a[j] + b[i] * b[j] + c[i] * c[j] + s[i] * s[j]) / 120.0;
    }

    if (std::fabs(vol6) < 1e-12) return mp; // open / degenerate: no enclosed volume

    // Auto-correct inward winding so volume is positive and the covariance keeps the right sign.
    if (vol6 < 0.0) {
        vol6 = -vol6;
        for (int i = 0; i < 3; ++i) { cAccum[i] = -cAccum[i]; for (int j = 0; j < 3; ++j) C[i][j] = -C[i][j]; }
    }

    mp.valid = true;
    mp.volume = vol6 / 6.0;
    for (int i = 0; i < 3; ++i) mp.centroid[i] = cAccum[i] / (4.0 * vol6);

    // Inertia about origin: I_ij = trace(C) d_ij - C_ij.
    const double trC = C[0][0] + C[1][1] + C[2][2];
    double Io[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            Io[i][j] = (i == j ? trC : 0.0) - C[i][j];

    // Parallel-axis shift to the centroid: I_cm = I_origin - m (|d|^2 I - d d^T).
    const double m = mp.volume, d[3] = {mp.centroid[0], mp.centroid[1], mp.centroid[2]};
    const double dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            mp.inertia[i][j] = Io[i][j] - m * ((i == j ? dd : 0.0) - d[i] * d[j]);

    return mp;
}

} // namespace maz::render
