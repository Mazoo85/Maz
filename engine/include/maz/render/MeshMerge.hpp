#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render MESH MERGE / CONCATENATE — glue several meshes into ONE mesh (one vertex buffer, one index buffer).
// This is the inverse of the M529 split-into-components and the workhorse of DRAW-CALL BATCHING: a scene with a
// hundred static props drawn as one combined mesh renders in a single draw call instead of a hundred, which is
// usually the biggest CPU win a renderer gets. It also backs "join selected" in an editor, flattening a set of
// pieces (each already placed via M571 applyTransform) into a single exportable object, and assembling procedural
// kit-bashed geometry from parts. Each source mesh's indices are re-based by the running vertex count so the
// triangles keep pointing at the right (now-appended) vertices; all vertex attributes (position, normal, UV,
// colour) come along unchanged. Header-only, pure CPU.
//
// Scope note (honest): this is a pure concatenation — it does NOT weld coincident vertices at the seams between
// parts (run M525 weldVertices / M559 autoWeld afterwards if you need a watertight join) and it does NOT re-pack
// or deduplicate, so merging N copies of a mesh yields N× the vertices. Winding and attributes are preserved
// exactly as given; if the parts disagree on winding, fix with M562 after merging. Empty inputs are skipped.
namespace maz::render {

// Merge all of `meshes` into one, appending vertices and re-basing each part's indices by the running offset.
inline shapes::MeshData mergeMeshes(const std::vector<shapes::MeshData>& meshes) {
    shapes::MeshData out;
    std::size_t totalV = 0, totalI = 0;
    for (const shapes::MeshData& m : meshes) {
        totalV += m.vertices.size();
        totalI += m.indices.size();
    }
    out.vertices.reserve(totalV);
    out.indices.reserve(totalI);

    for (const shapes::MeshData& m : meshes) {
        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.insert(out.vertices.end(), m.vertices.begin(), m.vertices.end());
        for (std::uint32_t idx : m.indices) {
            out.indices.push_back(idx + base); // re-base onto the appended vertex block
        }
    }
    return out;
}

// Convenience two-mesh overload.
inline shapes::MeshData mergeMeshes(const shapes::MeshData& a, const shapes::MeshData& b) {
    return mergeMeshes(std::vector<shapes::MeshData>{a, b});
}

} // namespace maz::render
