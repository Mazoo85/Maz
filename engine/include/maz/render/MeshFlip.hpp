#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>

// maz::render FLIP / REVERSE — deliberately turn a mesh inside-out: reverse every triangle's winding AND negate
// every vertex normal, so the surface faces the OTHER way. This is a different job from M562 winding-consistency
// (which only makes a mesh AGREE with itself): here you WANT the flip. Uses: build an inward-facing shell — a
// skybox, a room seen from inside, a cave interior, a hollow that culls its outer faces so you see the far walls;
// correct a whole model that imported inside-out in one call; or make a two-sided effect by MERGING (M572) a mesh
// with its flipped copy so both faces render under single-sided culling. Reversing the winding (swap the 2nd and
// 3rd corner of each triangle) flips which side back-face culling drops; negating the normals flips which way the
// surface shades. Header-only, pure CPU.
//
// Scope note (honest): this negates STORED vertex normals and reverses triangle order — it does not recompute
// normals from geometry (if a mesh has none, negating zero stays zero; run computeNormals first). Positions, UVs
// and colours are untouched. `flipWinding` alone reverses culling without touching shading; `flipNormals` alone
// re-shades without changing culling; `flipMesh` does both (the usual "make it face inward").
namespace maz::render {

// Reverse every triangle's winding (swap corners 2 and 3). Flips which face back-face culling keeps.
inline shapes::MeshData flipWinding(const shapes::MeshData& mesh) {
    shapes::MeshData out = mesh;
    for (std::size_t t = 0; t + 2 < out.indices.size(); t += 3) {
        const std::uint32_t tmp = out.indices[t + 1];
        out.indices[t + 1] = out.indices[t + 2];
        out.indices[t + 2] = tmp;
    }
    return out;
}

// Negate every stored vertex normal. Flips which way the surface shades without changing culling.
inline shapes::MeshData flipNormals(const shapes::MeshData& mesh) {
    shapes::MeshData out = mesh;
    for (MeshVertex& v : out.vertices) {
        v.nx = -v.nx;
        v.ny = -v.ny;
        v.nz = -v.nz;
    }
    return out;
}

// Turn a mesh fully inside-out: reverse winding AND negate normals (the usual "face inward" operation).
inline shapes::MeshData flipMesh(const shapes::MeshData& mesh) { return flipNormals(flipWinding(mesh)); }

} // namespace maz::render
