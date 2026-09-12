#pragma once

#include "maz/math/FitObb.hpp"              // math::detail::jacobiEigen3 (symmetric 3x3 eigensolve)
#include "maz/math/Math.hpp"                // math::vec3
#include "maz/render/MeshMassProperties.hpp" // computeMassProperties (volume, centroid, inertia tensor)
#include "maz/render/Shapes.hpp"            // shapes::MeshData

#include <cmath>
#include <cstddef>

// maz::render PRINCIPAL INERTIA AXES — the three natural spin axes of a solid mesh and how hard it is to spin
// about each. Every rigid body has three perpendicular axes about which it rotates cleanly (no wobble); a physics
// engine that wants realistic tumbling (a thrown plank spins easily end-over-end but resists rolling about its
// length) needs exactly this: the centre of mass, the three principal axes, and the moment of inertia about each.
// This takes the raw inertia TENSOR from `computeMassProperties` (M549) and DIAGONALISES it with the engine's
// symmetric Jacobi solver: the eigenvectors are the principal axes, the eigenvalues the principal moments. Axes
// come sorted by moment ascending, so `axis[0]` is the easiest to spin (the object's long direction) and `axis[2]`
// the hardest. Reuses `math::detail::jacobiEigen3`. Header-only, std-only, deterministic.
//
// Scope note (honest): correct only for a CLOSED, consistently-wound solid (the volume integral needs a
// watertight surface — an open mesh gives `valid=false`). Values are at unit density (mass == volume); scale the
// moments by real density for physical units. Reads positions only. A degenerate / zero-volume mesh is invalid.
namespace maz::render {

struct PrincipalAxes {
    bool valid = false;
    math::vec3 centre{0, 0, 0};   // centre of mass
    double mass = 0.0;             // = volume at unit density
    math::vec3 axis[3];           // three orthonormal principal axes (sorted by ascending moment)
    double moment[3] = {0, 0, 0}; // principal moment of inertia about each axis (ascending)
};

inline PrincipalAxes computePrincipalAxes(const shapes::MeshData& mesh) {
    PrincipalAxes out;
    const MassProperties mp = computeMassProperties(mesh);
    if (!mp.valid) return out;

    out.valid = true;
    out.centre = math::vec3(static_cast<float>(mp.centroid[0]), static_cast<float>(mp.centroid[1]),
                            static_cast<float>(mp.centroid[2]));
    out.mass = mp.volume;

    double a[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) a[i][j] = mp.inertia[i][j];

    double evec[3][3], eval[3];
    math::detail::jacobiEigen3(a, evec, eval);

    // Sort the three eigen-pairs by eigenvalue (moment) ascending.
    int order[3] = {0, 1, 2};
    for (int i = 0; i < 2; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (eval[order[j]] < eval[order[i]]) { const int tmp = order[i]; order[i] = order[j]; order[j] = tmp; }

    for (int k = 0; k < 3; ++k) {
        const int c = order[k];
        math::vec3 ax(static_cast<float>(evec[0][c]), static_cast<float>(evec[1][c]), static_cast<float>(evec[2][c]));
        const float len = std::sqrt(ax.x * ax.x + ax.y * ax.y + ax.z * ax.z);
        if (len > 1e-12f) ax = math::vec3(ax.x / len, ax.y / len, ax.z / len);
        out.axis[k] = ax;
        out.moment[k] = eval[c];
    }
    return out;
}

} // namespace maz::render
