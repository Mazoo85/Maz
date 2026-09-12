#pragma once

#include "maz/math/Math.hpp"      // math::vec3
#include "maz/render/MeshSlice.hpp" // sliceMesh, SliceContour
#include "maz/render/Shapes.hpp"   // shapes::MeshData

#include <cstddef>
#include <vector>

// maz::render SLAB SLICING — cut a mesh into a STACK of evenly-spaced cross-sections along one axis and return
// each layer's contour. This is what a 3D-printer / laser-cutter slicer does before it prints: chop the model
// into N horizontal slabs and trace the outline of each so the machine knows where to lay material or cut. It is
// also the way to build a "topographic" contour set (a hill drawn as stacked height rings), a stack of collision
// cross-sections, or a layered cutaway preview. It simply calls the engine's `sliceMesh` (M532) at N plane
// heights spanning the mesh's extent along the chosen axis and collects the resulting polyline loops per layer.
// Header-only, deterministic.
//
// Scope note (honest): each layer is exactly what `sliceMesh` returns (ordered loops, closed on a watertight
// solid). By default the N planes sample the layer CENTRES — heights (i+0.5)/N across the bounding box — which
// avoids landing a plane exactly on the flat top/bottom cap (where a coplanar face gives a degenerate contour);
// pass `sampleEdges=true` to place the planes at the layer boundaries instead. axis 0=X, 1=Y (default), 2=Z. A
// mesh with no extent along the axis, or count < 1, yields no layers.
namespace maz::render {

struct MeshLayers {
    int axis = 1;                          // the slicing axis (0=X, 1=Y, 2=Z)
    std::vector<float> heights;            // the axis coordinate of each slice plane
    std::vector<SliceContour> layers;      // one contour (loops) per height, in ascending order
};

// Slice `mesh` into `count` cross-sections along `axis`. Returns the plane heights and per-layer contours.
inline MeshLayers sliceLayers(const shapes::MeshData& mesh, int axis, int count, bool sampleEdges = false) {
    MeshLayers out;
    const int a = axis < 0 ? 0 : (axis > 2 ? 2 : axis);
    out.axis = a;
    if (count < 1 || mesh.vertices.empty()) return out;

    auto coord = [a](const MeshVertex& v) { return a == 0 ? v.px : (a == 1 ? v.py : v.pz); };
    float lo = coord(mesh.vertices[0]), hi = lo;
    for (const MeshVertex& v : mesh.vertices) {
        const float c = coord(v);
        if (c < lo) lo = c;
        if (c > hi) hi = c;
    }
    if (hi <= lo) return out; // no extent along this axis

    const math::vec3 normal = a == 0 ? math::vec3(1, 0, 0) : (a == 1 ? math::vec3(0, 1, 0) : math::vec3(0, 0, 1));
    const float span = hi - lo;

    out.heights.reserve(static_cast<std::size_t>(count));
    out.layers.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        float t;
        if (sampleEdges) {
            // Layer boundaries: N planes from just inside lo to just inside hi (endpoints nudged in when N>1).
            t = count == 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(count - 1);
        } else {
            t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count); // layer centres
        }
        const float h = lo + span * t;
        out.heights.push_back(h);
        // sliceMesh's plane is { x : dot(normal, x) = offset }; for an axis normal, offset == the height.
        out.layers.push_back(sliceMesh(mesh, normal, h));
    }
    return out;
}

} // namespace maz::render
