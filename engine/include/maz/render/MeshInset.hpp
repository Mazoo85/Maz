#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render INSET FACES — shrink every triangle IN PLACE toward its own centre, opening a gap between
// neighbouring faces. Each triangle keeps its shape and orientation but scales down about its centroid by
// `amount` (0 = untouched, 1 = collapsed to a point), so a solid surface becomes a field of shrunken tiles with
// dark seams between them: panel gaps on a spaceship hull, grout lines between floor tiles, a "greeble" panelled
// look, or the base ring for a per-face extrude/bevel. This is Blender's "Inset Faces → Individual" (the shrink
// part). It first UNWELDS (each triangle gets its own three corners carrying that triangle's flat face normal, so
// the tiles separate cleanly and flat-shade), then moves each corner a fraction `amount` of the way toward the
// triangle's centroid. Header-only, pure CPU.
//
// Scope note (honest): this insets each triangle INDEPENDENTLY (Blender's "individual faces" mode), so a flat
// region tiled by many triangles gets a seam along every interior edge, not just the region's outline — run it on
// a low-poly / quad-like mesh, or merge coplanar triangles first, if you want gaps only between real panels.
// Positions move toward the per-triangle centroid; the centroid itself is preserved (so the tile stays put, just
// smaller). `amount` may exceed 1 (overshoot through the centre) or go negative (grow the tile). Triples the
// vertex count (3× triangles, no sharing).
namespace maz::render {

// Return a copy of `mesh` with every triangle shrunk toward its own centroid by `amount`. Unwelds faces.
inline shapes::MeshData insetFaces(const shapes::MeshData& mesh, float amount) {
    shapes::MeshData out;
    const std::size_t triN = mesh.indices.size() / 3;
    out.vertices.reserve(triN * 3);
    out.indices.reserve(triN * 3);

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        const math::vec3 a = pos(ia), b = pos(ib), c = pos(ic);
        const math::vec3 g((a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f);

        // Flat face normal (unit; zero for a degenerate triangle).
        const math::vec3 cr = math::cross(b - a, c - a);
        const float len = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
        const math::vec3 n = len > 1e-20f ? math::vec3(cr.x / len, cr.y / len, cr.z / len) : math::vec3(0, 0, 0);

        const float s = 1.0f - amount; // scale about the centroid
        const std::uint32_t idx[3] = {ia, ib, ic};
        for (int k = 0; k < 3; ++k) {
            MeshVertex v = mesh.vertices[idx[static_cast<std::size_t>(k)]];
            const math::vec3 p = pos(idx[static_cast<std::size_t>(k)]);
            v.px = g.x + (p.x - g.x) * s;
            v.py = g.y + (p.y - g.y) * s;
            v.pz = g.z + (p.z - g.z) * s;
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
            const std::uint32_t nid = static_cast<std::uint32_t>(out.vertices.size());
            out.vertices.push_back(v);
            out.indices.push_back(nid);
        }
    }
    return out;
}

} // namespace maz::render
