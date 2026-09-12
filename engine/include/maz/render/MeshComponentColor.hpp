#pragma once

#include "maz/render/MeshComponents.hpp" // connectedComponentLabels
#include "maz/render/Shapes.hpp"         // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render COMPONENT TINT — paint every disconnected PIECE of a mesh a different colour, so you can SEE at a
// glance how many separate islands it is made of and which triangles belong together. A model that looks like one
// object is often secretly several (a character plus loose props, terrain chunks that never welded, stray shards
// from a bad boolean); tinting each connected component a distinct hue is the standard debug view for spotting
// that — "why is my one mesh actually 40 pieces?" — and for authoring per-part masks. Each vertex's colour is set
// from its component's index via evenly-spread hues (golden-ratio stepping so adjacent components never share a
// near-colour), at the given saturation/value. Reuses `connectedComponentLabels` (M517). Header-only, std-only,
// deterministic — the same mesh always gets the same colours.
//
// Scope note (honest): "connected" means sharing a vertex INDEX (welded topology) — two pieces touching in space
// but with separate vertices read as separate components (weld first with `weldVertices` if you want them merged).
// This overwrites the RGB of every vertex; positions, normals and UVs are untouched. Colours are opaque debug
// hues, not a physically meaningful signal.
namespace maz::render {

namespace detail {
// HSV (all in [0,1]) to RGB. Standard six-sector conversion.
inline void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    const float h6 = (h - std::floor(h)) * 6.0f;
    const int i = static_cast<int>(h6) % 6;
    const float f = h6 - std::floor(h6);
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}
} // namespace detail

// Return a copy of `mesh` with each connected component tinted a distinct hue. `count` (optional out) receives the
// number of components found.
inline shapes::MeshData tintComponents(const shapes::MeshData& mesh, float saturation = 0.7f, float value = 0.9f,
                                       std::uint32_t* count = nullptr) {
    shapes::MeshData out = mesh;
    std::uint32_t componentCount = 0;
    const std::vector<std::uint32_t> triLabel = connectedComponentLabels(mesh, componentCount); // per-triangle
    if (count) *count = componentCount;
    if (out.vertices.empty()) return out;

    // Spread the per-triangle island labels onto the vertices they reference (a vertex touched by no triangle
    // keeps label 0).
    std::vector<std::uint32_t> vertLabel(out.vertices.size(), 0);
    const std::size_t triCount = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triCount && t < triLabel.size(); ++t)
        for (int c = 0; c < 3; ++c) {
            const std::uint32_t vi = mesh.indices[t * 3 + static_cast<std::size_t>(c)];
            if (vi < vertLabel.size()) vertLabel[vi] = triLabel[t];
        }

    for (std::size_t i = 0; i < out.vertices.size(); ++i) {
        const float hue = std::fmod(static_cast<float>(vertLabel[i]) * 0.6180339887f, 1.0f); // golden-ratio spread
        float r = 1, g = 1, b = 1;
        detail::hsvToRgb(hue, saturation, value, r, g, b);
        out.vertices[i].r = r;
        out.vertices[i].g = g;
        out.vertices[i].b = b;
    }
    return out;
}

} // namespace maz::render
