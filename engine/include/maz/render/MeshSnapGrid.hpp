#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render SNAP-TO-GRID — round every vertex position onto a regular WORLD grid: pick a grid step (say 0.25
// units) and each vertex jumps to the nearest multiple of it. This is the "tidy up" pass for CAD-like or
// block/voxel-style meshes: it removes the tiny floating-point drift that creeps in from modelling, rotation, or
// import (so 1.0000001 and 0.9999998 both become a clean 1.0), and it makes vertices that were ALMOST at the
// same spot land EXACTLY on it — which then lets the bit-exact reindex (M575) actually fuse them. Godot modellers
// reach for "snap to grid" for exactly this kind of clean, aligned geometry.
//
// This is a DIFFERENT job from mesh quantization (`MeshQuantize`): that packs positions into N-bit integers
// relative to the mesh's bounding box to shrink the FILE (a compression/transport concern, with a per-mesh grid
// that changes with the box); this snaps to a fixed ABSOLUTE world grid you choose, purely to clean up the
// geometry — the vertices stay full 32-bit floats, just rounded. The report tells you how many vertices actually
// moved and by how much, so you can see whether the step was gentle (drift cleanup) or aggressive (block-ifying).
//
// Scope note (honest): this rounds POSITIONS only — normals, colours and UVs are untouched, so after an
// aggressive snap the stored normals may no longer match the flattened geometry (re-run `computeNormals` if the
// shading looks off). Snapping can pull two triangles' corners onto the same grid point and so create degenerate
// (zero-area) triangles; follow with `reindexMesh`/`weldVertices` to drop them. A grid step of 0 (or negative) on
// an axis leaves that axis untouched, so you can snap only X/Z (ground plane) and leave height free. Header-only,
// deterministic.
namespace maz::render {

struct SnapResult {
    shapes::MeshData mesh;            // the snapped mesh
    std::uint32_t movedVertices = 0;  // how many vertices ended up at a different position
    float maxDisplacement = 0.0f;     // the largest distance any single vertex moved
};

// Round each vertex position to the nearest point on a world grid of spacing `step` (per axis), with the grid
// lines passing through `origin`. A `step` component <= 0 leaves that axis unchanged.
inline SnapResult snapVerticesToGrid(const shapes::MeshData& mesh, math::vec3 step,
                                     math::vec3 origin = math::vec3(0, 0, 0)) {
    SnapResult out;
    out.mesh = mesh;

    auto snapAxis = [](float value, float o, float s) -> float {
        if (s <= 0.0f) return value; // this axis is left free
        return o + std::round((value - o) / s) * s;
    };

    for (MeshVertex& v : out.mesh.vertices) {
        const float nx = snapAxis(v.px, origin.x, step.x);
        const float ny = snapAxis(v.py, origin.y, step.y);
        const float nz = snapAxis(v.pz, origin.z, step.z);
        const float dx = nx - v.px, dy = ny - v.py, dz = nz - v.pz;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > 0.0f) {
            ++out.movedVertices;
            const float d = std::sqrt(d2);
            if (d > out.maxDisplacement) out.maxDisplacement = d;
        }
        v.px = nx;
        v.py = ny;
        v.pz = nz;
    }
    return out;
}

// Convenience: the same grid step on all three axes.
inline SnapResult snapVerticesToGrid(const shapes::MeshData& mesh, float step,
                                     math::vec3 origin = math::vec3(0, 0, 0)) {
    return snapVerticesToGrid(mesh, math::vec3(step, step, step), origin);
}

} // namespace maz::render
