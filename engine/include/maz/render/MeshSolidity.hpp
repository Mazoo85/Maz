#pragma once

#include "maz/game/ConvexHull3D.hpp"           // buildConvexHull, ConvexHull3D (M292-era)
#include "maz/math/Math.hpp"                    // math::vec3, cross, dot
#include "maz/render/MeshMassProperties.hpp"    // computeMassProperties, MassProperties (M532)
#include "maz/render/Shapes.hpp"                // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render MESH SOLIDITY (convexity) RATIO — how CONVEX is a shape? Wrap the mesh in its convex hull (the
// tightest shape with no dents — imagine shrink-wrapping it) and compare the mesh's own enclosed volume to the
// hull's. The ratio (mesh volume ÷ hull volume) is 1.0 for a perfectly convex solid (a cube, a ball, a die) and
// drops toward 0 the more the shape caves in — a bowl, a cog, a chair, a tree. This "solidity" is a one-number
// convexity score meshing and analysis tools report: it drives LOD/collision decisions (a near-convex prop can
// use its cheap hull as a collider), flags whether a boolean/CSG result stayed solid, and classifies shapes
// (blobby vs branchy) for procedural placement. Reuses the M532 mass-properties volume and the M292 convex-hull
// builder; the hull's volume comes from summing signed tetrahedra over its faces. Header-only, pure CPU.
//
// Scope note (honest): the mesh volume needs a CLOSED, consistently-wound surface (an open shell reports invalid
// — weld and orient it first via M525/M562); winding sign is auto-corrected so an inside-out but closed mesh
// still measures. The hull is built from the mesh's vertices, so solidity captures concavity of the VERTEX cloud,
// not sub-vertex surface ripple. Numerically solidity can sit a hair above 1.0 on a convex mesh from floating-
// point; clamp if you need a strict [0,1].
namespace maz::render {

struct SolidityReport {
    bool valid = false;       // false if the mesh has no closed volume or the hull is degenerate
    double meshVolume = 0.0;  // enclosed volume of the mesh (unit density)
    double hullVolume = 0.0;  // volume of the convex hull of the mesh's vertices
    double solidity = 0.0;    // meshVolume / hullVolume in (0,1]; 1 = convex, smaller = more concave
};

// Compute the solidity (convexity) ratio of `mesh`.
inline SolidityReport analyzeSolidity(const shapes::MeshData& mesh) {
    SolidityReport rep;
    if (mesh.vertices.size() < 4 || mesh.indices.size() < 12) return rep; // need a 3D closed solid

    const MassProperties mp = computeMassProperties(mesh);
    if (!mp.valid || mp.volume <= 0.0) return rep;

    // Convex hull of the mesh's vertex positions.
    std::vector<math::vec3> pts(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const MeshVertex& v = mesh.vertices[i];
        pts[i] = math::vec3(v.px, v.py, v.pz);
    }
    const game::ConvexHull3D hull = game::buildConvexHull(pts);
    if (!hull.valid || hull.faces.empty()) return rep;

    // Hull volume = |sum of signed tetrahedra (origin, a, b, c)| / 6 over its outward-wound faces.
    double vol6 = 0.0;
    for (const game::HullFace& f : hull.faces) {
        const math::vec3& a = pts[static_cast<std::size_t>(f.a)];
        const math::vec3& b = pts[static_cast<std::size_t>(f.b)];
        const math::vec3& c = pts[static_cast<std::size_t>(f.c)];
        vol6 += static_cast<double>(math::dot(a, math::cross(b, c)));
    }
    const double hullVol = std::fabs(vol6) / 6.0;
    if (hullVol <= 1e-12) return rep;

    rep.valid = true;
    rep.meshVolume = mp.volume;
    rep.hullVolume = hullVol;
    rep.solidity = mp.volume / hullVol;
    return rep;
}

} // namespace maz::render
