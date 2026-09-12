#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::render NORMALIZE TO A TARGET BOX — recentre AND uniformly scale a mesh so it fills a chosen box. Imported
// models arrive at wildly different scales — one in metres, one in centimetres, one a thousand units tall — and
// off-centre to boot. This is the import-normalization companion to the M569 pivot snap: it fits any mesh into a
// consistent size (default: centred in a unit cube, longest side = 1) so a whole asset library shares one scale
// and pivot, thumbnails frame identically, and downstream tools (voxelize, SDF, sampling) get predictable extents.
// The scale is UNIFORM (one factor on all axes) so the shape never distorts; `fitLongest` scales the longest side
// to the target (the mesh fits inside the box) while the alternative fills every axis. Returns the transformed
// copy plus the exact scale and translation applied, so an inverse or a parent transform can undo it. Only
// vertex POSITIONS change. Reuses the mesh bounding box; header-only, pure CPU.
//
// Scope note (honest): UNIFORM scale preserves proportions — the mesh is centred in the target box and touches it
// on its longest axis (fitLongest=true) or exactly fills it only if it already matches the box's aspect; it does
// NOT stretch to fill a non-cubic box on every axis (that would distort). Normals/UVs/colours are untouched
// (uniform scale keeps normals valid). A zero-extent (single-point or empty) mesh is returned unchanged with unit
// scale. This translates+scales only — it never rotates (align first via M568 if needed).
namespace maz::render {

struct NormalizeResult {
    shapes::MeshData mesh;         // transformed copy
    float scale = 1.0f;            // uniform scale applied (newPos = (oldPos − centre)*scale + targetCentre)
    math::vec3 sourceCentre{0, 0, 0}; // bounding-box centre of the input
    math::vec3 targetCentre{0, 0, 0}; // centre the mesh was placed at
};

// Fit `mesh` into a box of full size `targetSize` centred at `targetCentre`. With `fitLongest` (default), the
// mesh's LONGEST side is scaled to the smallest target dimension so it fits inside; otherwise the same longest-
// side rule uses the target's largest dimension. Uniform scale — proportions are preserved.
inline NormalizeResult normalizeToBox(const shapes::MeshData& mesh, math::vec3 targetSize = math::vec3(1, 1, 1),
                                      math::vec3 targetCentre = math::vec3(0, 0, 0), bool fitLongest = true) {
    NormalizeResult out;
    out.mesh = mesh;
    out.targetCentre = targetCentre;
    const std::size_t n = mesh.vertices.size();
    if (n == 0) return out;

    math::vec3 lo(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz), hi = lo;
    for (const MeshVertex& v : mesh.vertices) {
        lo = math::vec3(std::min(lo.x, v.px), std::min(lo.y, v.py), std::min(lo.z, v.pz));
        hi = math::vec3(std::max(hi.x, v.px), std::max(hi.y, v.py), std::max(hi.z, v.pz));
    }
    const math::vec3 centre = (lo + hi) * 0.5f;
    out.sourceCentre = centre;

    const math::vec3 ext = hi - lo;
    const float longestSide = std::max(ext.x, std::max(ext.y, ext.z));
    if (longestSide <= 1e-20f) {
        // Zero-extent mesh (a point): just translate its centre to the target, no scaling.
        for (MeshVertex& v : out.mesh.vertices) {
            v.px += targetCentre.x - centre.x;
            v.py += targetCentre.y - centre.y;
            v.pz += targetCentre.z - centre.z;
        }
        return out;
    }
    // Target reference length: the smallest target dimension keeps the mesh INSIDE the box; largest fills it.
    const float targetRef = fitLongest ? std::min(targetSize.x, std::min(targetSize.y, targetSize.z))
                                       : std::max(targetSize.x, std::max(targetSize.y, targetSize.z));
    const float scale = targetRef / longestSide;
    out.scale = scale;

    for (MeshVertex& v : out.mesh.vertices) {
        v.px = (v.px - centre.x) * scale + targetCentre.x;
        v.py = (v.py - centre.y) * scale + targetCentre.y;
        v.pz = (v.pz - centre.z) * scale + targetCentre.z;
    }
    return out;
}

} // namespace maz::render
