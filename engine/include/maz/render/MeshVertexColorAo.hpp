#pragma once

#include "maz/render/MeshAmbientOcclusion.hpp" // bakeVertexAO (M533)
#include "maz/render/Shapes.hpp"               // shapes::MeshData, MeshVertex

#include <cstddef>
#include <vector>

// maz::render VERTEX-COLOR AMBIENT-OCCLUSION BAKE — darken each vertex's stored RGB by how OCCLUDED it is, so
// the mesh carries its own soft contact shadows with no texture, no lightmap, and no runtime lighting. AO is
// the free ambient shadowing of nooks and crevices — under a ledge, inside a fold, where two walls meet — and
// baking it straight into vertex colours is the cheapest way to give flat-lit or mobile/retro content that
// grounded, hand-painted look (it is exactly what "bake AO to vertex colours" does in Blender, and what a lot
// of low-poly games ship). Reuses the M533 hemisphere raycaster (`bakeVertexAO`, 0 = open, 1 = fully occluded)
// and multiplies each channel by (1 − ao·strength): open surfaces keep their colour, creases go dark. Returns a
// copy; the source is untouched. Header-only, deterministic (golden-angle sampling), headless.
//
// Scope note (honest): this MULTIPLIES existing vertex colours, so a white mesh becomes a pure AO map and a
// tinted mesh keeps its tint times the shadow — call it once (baking twice double-darkens). AO smoothness is
// limited by mesh tessellation (it is a per-VERTEX value); for crisp AO on low-poly meshes bake to a texture
// instead. `strength` in [0,1] scales the darkening; `rayCount`/`maxDistance` are the M533 knobs.
namespace maz::render {

// Return a copy of `mesh` with per-vertex AO multiplied into the RGB. `strength` (0..1) scales how dark the
// occluded areas get; `rayCount` hemisphere samples per vertex; `maxDistance` limits occluder range.
inline shapes::MeshData bakeAoToVertexColor(const shapes::MeshData& mesh, float strength = 1.0f,
                                            int rayCount = 64, float maxDistance = 1e30f) {
    shapes::MeshData out = mesh;
    if (out.vertices.empty() || out.indices.size() < 3) return out;

    const std::vector<float> ao = bakeVertexAO(mesh, rayCount, maxDistance);
    const float s = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    for (std::size_t i = 0; i < out.vertices.size() && i < ao.size(); ++i) {
        const float lit = 1.0f - ao[i] * s; // open -> 1, fully occluded -> 1-strength
        out.vertices[i].r *= lit;
        out.vertices[i].g *= lit;
        out.vertices[i].b *= lit;
    }
    return out;
}

} // namespace maz::render
