#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render EXPLODE — pull a mesh's triangles APART: give every triangle its own three vertices, then shove each
// triangle bodily OUTWARD so the surface blooms open like an exploded-view diagram. Two flavours:
//   • `explodeFaces` pushes each triangle along its OWN face normal by `distance` — the surface puffs out along
//     the direction each face already points (a cube's six sides slide straight out, a sphere's facets bristle
//     outward). This is the "peel a model apart to see it / dissolve / shatter-bloom" effect games use for
//     unlock reveals, deaths, and assembly animations, and animating `distance` 0→D is the whole effect.
//   • `explodeFacesRadial` pushes each triangle away from a CENTRE point (the bbox centre by default) by
//     `distance` — every piece flies outward from the middle regardless of which way it faces, the classic
//     exploded-parts look.
// Both first UNWELD (each triangle gets private corners carrying that triangle's flat face normal, exactly like
// the M-facet split) so neighbouring triangles separate cleanly instead of dragging shared corners; that also
// makes the exploded pieces flat-shade correctly. Header-only, pure CPU.
//
// Scope note (honest): this multiplies the vertex count by 3× the triangle count (no sharing — that is the point;
// re-weld/`reindexMesh` if you set distance back to 0 and want the compact mesh back). Positions and normals are
// rewritten; UVs and colours ride along per corner. Degenerate (zero-area) triangles have no defined normal, so
// they are copied in place with no offset. Distance is in world units and may be negative (implode inward).
namespace maz::render {

namespace detail {

// The flat face normal of triangle `t` (normalized cross of its edges); returns (0,0,0) for a degenerate face.
inline math::vec3 explodeFaceNormal(const shapes::MeshData& mesh, std::size_t t) {
    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };
    const math::vec3 a = pos(mesh.indices[t * 3 + 0]);
    const math::vec3 b = pos(mesh.indices[t * 3 + 1]);
    const math::vec3 c = pos(mesh.indices[t * 3 + 2]);
    const math::vec3 cr = math::cross(b - a, c - a);
    const float len = std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
    if (len <= 1e-20f) return math::vec3(0, 0, 0);
    return math::vec3(cr.x / len, cr.y / len, cr.z / len);
}

// Emit triangle `t`'s three corners into `out`, offset by `off`, each stamped with normal `nrm`.
inline void emitExplodedTri(const shapes::MeshData& mesh, std::size_t t, math::vec3 off, math::vec3 nrm,
                            shapes::MeshData& out) {
    for (int c = 0; c < 3; ++c) {
        MeshVertex v = mesh.vertices[mesh.indices[t * 3 + static_cast<std::size_t>(c)]];
        v.px += off.x;
        v.py += off.y;
        v.pz += off.z;
        v.nx = nrm.x;
        v.ny = nrm.y;
        v.nz = nrm.z;
        const std::uint32_t idx = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(v);
        out.indices.push_back(idx);
    }
}

} // namespace detail

// Push each triangle along its own face normal by `distance`. Unwelds faces; degenerate faces are left in place.
inline shapes::MeshData explodeFaces(const shapes::MeshData& mesh, float distance) {
    shapes::MeshData out;
    const std::size_t triN = mesh.indices.size() / 3;
    out.vertices.reserve(triN * 3);
    out.indices.reserve(triN * 3);
    for (std::size_t t = 0; t < triN; ++t) {
        const math::vec3 n = detail::explodeFaceNormal(mesh, t);
        const math::vec3 off(n.x * distance, n.y * distance, n.z * distance);
        detail::emitExplodedTri(mesh, t, off, n, out);
    }
    return out;
}

// Push each triangle away from `centre` by `distance` (each face moves along the direction from `centre` to its
// own centroid). A face sitting exactly on `centre` falls back to its face normal. Unwelds faces.
inline shapes::MeshData explodeFacesRadial(const shapes::MeshData& mesh, float distance, math::vec3 centre) {
    shapes::MeshData out;
    const std::size_t triN = mesh.indices.size() / 3;
    out.vertices.reserve(triN * 3);
    out.indices.reserve(triN * 3);
    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };
    for (std::size_t t = 0; t < triN; ++t) {
        const math::vec3 a = pos(mesh.indices[t * 3 + 0]);
        const math::vec3 b = pos(mesh.indices[t * 3 + 1]);
        const math::vec3 c = pos(mesh.indices[t * 3 + 2]);
        const math::vec3 centroid((a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f);
        math::vec3 dir(centroid.x - centre.x, centroid.y - centre.y, centroid.z - centre.z);
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        const math::vec3 n = detail::explodeFaceNormal(mesh, t);
        if (len > 1e-20f) {
            dir = math::vec3(dir.x / len, dir.y / len, dir.z / len);
        } else {
            dir = n; // face at the centre: fall back to its own normal
        }
        const math::vec3 off(dir.x * distance, dir.y * distance, dir.z * distance);
        detail::emitExplodedTri(mesh, t, off, n, out); // keep the flat face normal for shading
    }
    return out;
}

// Radial explode about the mesh's bounding-box centre.
inline shapes::MeshData explodeFacesRadial(const shapes::MeshData& mesh, float distance) {
    if (mesh.vertices.empty()) return shapes::MeshData{};
    math::vec3 lo(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
    math::vec3 hi = lo;
    for (const MeshVertex& v : mesh.vertices) {
        lo = math::vec3(v.px < lo.x ? v.px : lo.x, v.py < lo.y ? v.py : lo.y, v.pz < lo.z ? v.pz : lo.z);
        hi = math::vec3(v.px > hi.x ? v.px : hi.x, v.py > hi.y ? v.py : hi.y, v.pz > hi.z ? v.pz : hi.z);
    }
    return explodeFacesRadial(mesh, distance, math::vec3((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f,
                                                         (lo.z + hi.z) * 0.5f));
}

} // namespace maz::render
