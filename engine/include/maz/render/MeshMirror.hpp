#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render MESH MIRROR / SYMMETRIZE — reflect a mesh across an axis-aligned plane and JOIN the reflection to
// the original, producing a symmetric whole: the "mirror modifier" every DCC tool has. Model one wing / one
// half of a face / the left side of a spaceship, mirror it, and get a seamless symmetric result — the standard
// modelling shortcut, also used to symmetrize a slightly-off scan and to fold a full mesh's data onto one half.
// Reflecting flips handedness, so each mirrored triangle's winding is REVERSED (and its baked normal negated)
// to keep faces pointing outward. Vertices lying on the mirror plane (within `weldEps`) are shared, not
// duplicated, so the seam is watertight (welded to the matching original vertex). `axis` picks the plane
// normal (0=X,1=Y,2=Z), `planeCoord` its position along that axis. Header-only, std-only, deterministic.
//
// Scope note (honest): welds only along the mirror plane (the seam) — it does not re-weld the two halves
// elsewhere or dedup interior duplicates; run MeshCleanup/MeshWeld afterwards if the source itself had
// coincident verts. Vertices on the far side of the plane are still mirrored (the modifier assumes the source
// sits on one side); clip first if you need a strict half.
namespace maz::render {

// Mirror `mesh` across the plane {axis == planeCoord} and return original ∪ reflection with the seam welded.
// axis: 0 = X plane (x = planeCoord), 1 = Y, 2 = Z.
inline shapes::MeshData mirrorMesh(const shapes::MeshData& mesh, int axis, float planeCoord,
                                   float weldEps = 1e-5f) {
    shapes::MeshData out = mesh; // start with the original half
    if (mesh.vertices.empty() || axis < 0 || axis > 2) return out;

    const std::uint32_t origVerts = static_cast<std::uint32_t>(mesh.vertices.size());
    auto coord = [axis](const MeshVertex& v) { return axis == 0 ? v.px : (axis == 1 ? v.py : v.pz); };

    // For each original vertex, decide its mirrored counterpart index: a seam vertex maps to ITSELF (shared),
    // any other maps to a freshly appended reflected vertex.
    std::vector<std::uint32_t> mirrorOf(origVerts);
    for (std::uint32_t i = 0; i < origVerts; ++i) {
        const MeshVertex& src = mesh.vertices[i];
        if (std::fabs(coord(src) - planeCoord) <= weldEps) {
            mirrorOf[i] = i; // on the plane -> shared seam vertex, not duplicated
            continue;
        }
        MeshVertex m = src;
        // Reflect position across the plane and negate the normal component along the axis.
        if (axis == 0) { m.px = 2.0f * planeCoord - src.px; m.nx = -src.nx; }
        else if (axis == 1) { m.py = 2.0f * planeCoord - src.py; m.ny = -src.ny; }
        else { m.pz = 2.0f * planeCoord - src.pz; m.nz = -src.nz; }
        mirrorOf[i] = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(m);
    }

    // Append reflected triangles with REVERSED winding (reflection flips orientation).
    const std::size_t triN = mesh.indices.size() / 3;
    out.indices.reserve(mesh.indices.size() * 2);
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t a = mesh.indices[t * 3 + 0];
        const std::uint32_t b = mesh.indices[t * 3 + 1];
        const std::uint32_t c = mesh.indices[t * 3 + 2];
        if (a >= origVerts || b >= origVerts || c >= origVerts) continue;
        out.indices.push_back(mirrorOf[a]);
        out.indices.push_back(mirrorOf[c]); // swap b/c to reverse winding
        out.indices.push_back(mirrorOf[b]);
    }
    return out;
}

} // namespace maz::render
