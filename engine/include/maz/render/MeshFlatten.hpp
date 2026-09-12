#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render FLATTEN / PROJECT-TO-PLANE — squash a mesh toward a flat plane: each vertex slides along the plane's
// normal toward its perpendicular projection onto the plane, blended by `t`. At t=0 nothing moves; at t=1 every
// vertex lands exactly on the plane (a pancake); in between the shape squashes smoothly. Uses: a cheap
// drop-shadow / blob-shadow caster (flatten a copy of a model onto the ground plane and render it dark), a decal
// or sticker baked onto a surface, a "pressed flat" squash-and-stretch pose, or projecting a prop onto a wall.
// The plane is a point + a normal; the normal is normalised internally so any length works. Header-only, CPU.
//
// Scope note (honest): this moves POSITIONS only, straight along the plane normal — normals are left as they were,
// so a fully-flattened mesh keeps its original (now wrong) shading; for a lit pancake set the normals to the plane
// normal or re-run `computeNormals` afterwards. At t=1 the mesh is coplanar and has zero thickness (its two sides
// overlap — expect z-fighting if both are drawn; it is meant as a shadow/decal source, not a solid). `t` may
// exceed 1 (overshoot past the plane) or go negative (push away from it).
namespace maz::render {

// Move each vertex toward its projection onto the plane through `planePoint` with normal `planeNormal`, by factor
// `t`. Returns a flattened copy. A zero-length normal is a no-op (no plane to project onto).
inline shapes::MeshData projectToPlane(const shapes::MeshData& mesh, math::vec3 planePoint, math::vec3 planeNormal,
                                       float t = 1.0f) {
    shapes::MeshData out = mesh;
    const float nlen = std::sqrt(planeNormal.x * planeNormal.x + planeNormal.y * planeNormal.y +
                                 planeNormal.z * planeNormal.z);
    if (nlen <= 1e-12f) return out;
    const math::vec3 n(planeNormal.x / nlen, planeNormal.y / nlen, planeNormal.z / nlen);

    for (MeshVertex& v : out.vertices) {
        // Signed distance from the plane along the normal.
        const float dist = (v.px - planePoint.x) * n.x + (v.py - planePoint.y) * n.y + (v.pz - planePoint.z) * n.z;
        const float move = -dist * t; // t=1 removes the full distance -> vertex lands on the plane
        v.px += n.x * move;
        v.py += n.y * move;
        v.pz += n.z * move;
    }
    return out;
}

} // namespace maz::render
