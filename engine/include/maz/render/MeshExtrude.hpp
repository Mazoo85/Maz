#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render EXTRUDE FACES — raise every triangle off the surface into a little standing prism: each face is
// pushed OUT along its own normal by `distance` and the gap it leaves is walled in on all three sides, so a flat
// panel sprouts a field of raised studs / buttons / greebles / brick-relief. This is Blender's "Extrude Individual
// Faces" and the workhorse for turning a plain surface into panelled sci-fi hull detail, chunky pixel-art relief,
// or the raised keys of a keypad. Each input triangle becomes a self-contained prism: its TOP (the triangle moved
// out by `distance`, still facing the same way) plus three SIDE walls (a quad per original edge, bridging the base
// edge to the raised edge) — 7 triangles from 1. Header-only, pure CPU.
//
// Scope note (honest): this extrudes each triangle INDEPENDENTLY (individual-faces mode) and unwelds, so a flat
// region tiled by many triangles raises each triangle as its own separate stud with walls along every interior
// edge, not one merged block — run it on a low-poly / already-panelled mesh, or merge coplanar triangles first,
// for clean single studs. Normals are set to each prism's TOP face normal (the side walls therefore shade like
// the top; re-run `computeNormals` for correct wall shading). The base outline stays where it was; `distance` may
// be negative to press faces INward. Multiplies the triangle count by 7 and the vertex count by 6.
namespace maz::render {

// Return a copy of `mesh` with every triangle extruded outward along its face normal by `distance` (top + 3 side
// walls per triangle). Unwelds.
inline shapes::MeshData extrudeFaces(const shapes::MeshData& mesh, float distance) {
    shapes::MeshData out;
    const std::size_t triN = mesh.indices.size() / 3;
    out.vertices.reserve(triN * 6);
    out.indices.reserve(triN * 21);

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        const math::vec3 a = pos(ia), b = pos(ib), c = pos(ic);

        // Flat face normal (unit; zero for a degenerate triangle -> no offset).
        const math::vec3 cr = math::cross(b - a, c - a);
        const float len = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
        const math::vec3 n = len > 1e-20f ? math::vec3(cr.x / len, cr.y / len, cr.z / len) : math::vec3(0, 0, 0);
        const math::vec3 off(n.x * distance, n.y * distance, n.z * distance);

        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        // Six vertices: base a,b,c (0,1,2) then top a',b',c' (3,4,5). All carry the top face normal.
        const std::uint32_t src[3] = {ia, ib, ic};
        for (int k = 0; k < 3; ++k) { // base ring
            MeshVertex v = mesh.vertices[src[static_cast<std::size_t>(k)]];
            v.nx = n.x; v.ny = n.y; v.nz = n.z;
            out.vertices.push_back(v);
        }
        for (int k = 0; k < 3; ++k) { // raised ring
            MeshVertex v = mesh.vertices[src[static_cast<std::size_t>(k)]];
            v.px += off.x; v.py += off.y; v.pz += off.z;
            v.nx = n.x; v.ny = n.y; v.nz = n.z;
            out.vertices.push_back(v);
        }

        auto tri = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z) {
            out.indices.push_back(base + x);
            out.indices.push_back(base + y);
            out.indices.push_back(base + z);
        };
        // Top face (same winding as the original, on the raised ring).
        tri(3, 4, 5);
        // Three side walls: base edge (p,q) -> raised (p',q'), split into two triangles each.
        tri(0, 1, 4); tri(0, 4, 3); // edge a-b
        tri(1, 2, 5); tri(1, 5, 4); // edge b-c
        tri(2, 0, 3); tri(2, 3, 5); // edge c-a
    }
    return out;
}

} // namespace maz::render
