#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render SPHERIFY / CAST-TO-SPHERE — inflate a mesh toward a perfect sphere: each vertex is pulled from where
// it is toward the point on a sphere of radius `radius` (about `centre`) that lies along its own direction from
// the centre, blended by `t`. At t=0 nothing moves; at t=1 every vertex sits exactly on the sphere; in between the
// shape smoothly rounds out. This is Blender's "Cast" modifier (sphere target) and the classic way to round a
// blocky low-poly shape — turn a subdivided cube into a ball, puff an angular rock smooth, or morph between a
// boxy and a rounded silhouette by animating `t`. Position AND normal blend toward the radial (outward-from-
// centre) direction, so the lighting rounds out with the shape. Header-only, pure CPU, deterministic.
//
// Scope note (honest): the roundness you get is limited by the mesh's tessellation — spherifying an 8-vertex cube
// just moves its 8 corners onto the sphere (still an octahedron-ish shell); subdivide first (Subdivision) so there
// are enough vertices to actually read as round. A vertex sitting exactly at `centre` has no direction to cast
// along, so it is left in place. `t` outside [0,1] is allowed (t>1 overshoots past the sphere, t<0 pushes inward
// away from it); the usual range is 0..1.
namespace maz::render {

// Blend `mesh` toward a sphere of `radius` about `centre` by factor `t`. Returns a spherified copy.
inline shapes::MeshData spherifyMesh(const shapes::MeshData& mesh, float radius, float t,
                                     math::vec3 centre = math::vec3(0, 0, 0)) {
    shapes::MeshData out = mesh;
    for (MeshVertex& v : out.vertices) {
        const float dx = v.px - centre.x, dy = v.py - centre.y, dz = v.pz - centre.z;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d <= 1e-12f) continue; // a vertex at the centre has no cast direction

        const float inv = 1.0f / d;
        const float rx = dx * inv, ry = dy * inv, rz = dz * inv; // unit radial direction

        // Position: magnitude lerps from d to `radius` along the same ray, so the vertex moves toward the sphere.
        const float m = d + (radius - d) * t;
        v.px = centre.x + rx * m;
        v.py = centre.y + ry * m;
        v.pz = centre.z + rz * m;

        // Normal: blend toward the outward radial direction, then renormalize.
        float nx = v.nx + (rx - v.nx) * t;
        float ny = v.ny + (ry - v.ny) * t;
        float nz = v.nz + (rz - v.nz) * t;
        const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen > 1e-12f) { nx /= nlen; ny /= nlen; nz /= nlen; }
        v.nx = nx;
        v.ny = ny;
        v.nz = nz;
    }
    return out;
}

} // namespace maz::render
