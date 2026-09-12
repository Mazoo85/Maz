#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render PROJECTION UV UNWRAP — auto-generate texture coordinates without a hand-made unwrap, by PROJECTING
// world positions onto a plane. Two flavours: PLANAR projection drops every vertex straight down one axis (the
// top-down "decal"/terrain map — paint a whole landscape or floor with one texture), and BOX projection picks,
// per triangle, the axis its face points most toward and projects onto that plane (the "cube"/triplanar unwrap
// Blender and Maya offer as an instant UV for hard-surface props — each face gets sensible, low-distortion
// coordinates with no manual seam work). This is the quick UV a mesh needs before it can show a tiled material,
// a decal, or a checker map for inspection. Planar keeps the mesh topology (UVs written in place); box must
// give each triangle its own corners (a face's projection axis differs from its neighbour's), so it returns a
// facet-split mesh like MeshFacet. Reuses only vec3 from maz::math. Header-only, deterministic.
//
// Scope note (honest): projection UVs stretch on surfaces steep to the projection axis (planar) or seam at the
// 45° between box axes — that is inherent to projection unwraps, not a bug; for low distortion on organic shapes
// an angle-based/LSCM unwrap is the heavier follow-up. Box projection maps opposite faces the same way (no
// back-face flip), the simple convention.
namespace maz::render {

namespace detail {
// Natural (u,v) world-axis channels for a projection down `axis`: X view -> (z,y), Y (top-down) -> (x,z),
// Z (front) -> (x,y). Keeps textures upright and un-mirrored for the common views.
inline void uvChannels(int axis, int& c0, int& c1) {
    if (axis == 0) { c0 = 2; c1 = 1; }        // X: u=z, v=y
    else if (axis == 1) { c0 = 0; c1 = 2; }   // Y: u=x, v=z
    else { c0 = 0; c1 = 1; }                   // Z: u=x, v=y
}
} // namespace detail

// Planar projection along `axis` (0=X,1=Y,2=Z): u,v are the other two world coordinates, scaled and offset.
// Topology is unchanged — UVs are written onto the existing vertices.
inline shapes::MeshData planarUv(const shapes::MeshData& mesh, int axis, float scale,
                                 float uOffset = 0.0f, float vOffset = 0.0f) {
    shapes::MeshData out = mesh;
    if (axis < 0 || axis > 2) return out;
    int c0 = 0, c1 = 0;
    detail::uvChannels(axis, c0, c1);
    for (MeshVertex& v : out.vertices) {
        const float p[3] = {v.px, v.py, v.pz};
        v.u = p[c0] * scale + uOffset;
        v.v = p[c1] * scale + vOffset;
    }
    return out;
}

// Box (triplanar) projection: each triangle is projected onto the plane of its dominant face-normal axis.
// Returns a facet-split mesh (3 vertices per triangle) so neighbouring faces can use different projection axes.
inline shapes::MeshData boxUv(const shapes::MeshData& mesh, float scale,
                              float uOffset = 0.0f, float vOffset = 0.0f) {
    shapes::MeshData out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0 || mesh.vertices.empty()) return out;
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
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) continue;

        const math::vec3 n = math::cross(pos(ib) - pos(ia), pos(ic) - pos(ia));
        const float ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
        int axis = 0;
        if (ay >= ax && ay >= az) axis = 1;
        else if (az >= ax && az >= ay) axis = 2;
        int c0 = 0, c1 = 0;
        detail::uvChannels(axis, c0, c1);

        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        for (std::uint32_t src : {ia, ib, ic}) {
            MeshVertex v = mesh.vertices[src];
            const float p[3] = {v.px, v.py, v.pz};
            v.u = p[c0] * scale + uOffset;
            v.v = p[c1] * scale + vOffset;
            out.vertices.push_back(v);
        }
        out.indices.push_back(base);
        out.indices.push_back(base + 1);
        out.indices.push_back(base + 2);
    }
    return out;
}

} // namespace maz::render
