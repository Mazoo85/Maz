#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render FLAT-SHADING FACET SPLIT — rebuild a mesh so every triangle owns its three OWN vertices, each
// carrying that triangle's FACE normal. Because no vertex is shared between faces, the lighting can't blend
// across edges, so the surface renders faceted — every triangle a crisp flat plane. This is Blender's
// "Shade Flat" / the low-poly look: a sphere becomes a geodesic gem, terrain becomes stylized facets, and it
// is also the honest way to export a mesh whose faces really are flat (a cube should NOT have its corners
// smoothed). The inverse of smooth (shared-vertex, averaged-normal) shading — pair with computeNormals for the
// smooth version. Positions/colours/UVs are copied per corner; only the normal is replaced with the face
// normal. Header-only, std-only, deterministic — output has exactly 3·triangleCount vertices.
//
// Scope note (honest): this INFLATES the vertex count (no sharing), which is the point — it is a display/export
// transform, not an optimization. Degenerate (zero-area) triangles get a zero normal (no valid facing) but are
// still emitted so the triangle set is preserved; drop them first with MeshCleanup if unwanted.
namespace maz::render {

// Return a flat-shaded copy of `mesh`: every triangle unwelded to three vertices with the triangle's face
// normal. An empty/degenerate index buffer yields an empty mesh.
inline shapes::MeshData facetMesh(const shapes::MeshData& mesh) {
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
        const float len = std::sqrt(math::dot(n, n));
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (len > 1e-20f) { nx = n.x / len; ny = n.y / len; nz = n.z / len; }

        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        for (std::uint32_t src : {ia, ib, ic}) {
            MeshVertex v = mesh.vertices[src]; // copy position/colour/uv
            v.nx = nx; v.ny = ny; v.nz = nz;   // replace with the shared face normal
            out.vertices.push_back(v);
        }
        out.indices.push_back(base);
        out.indices.push_back(base + 1);
        out.indices.push_back(base + 2);
    }
    return out;
}

} // namespace maz::render
