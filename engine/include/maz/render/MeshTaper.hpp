#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render TAPER — squeeze or fan a mesh along an axis: the cross-section perpendicular to the axis is scaled
// by a factor that ramps LINEARLY from `startScale` (at the low end of the axis) to `endScale` (at the high end),
// so a straight bar cones into a pyramid or spike, a cylinder becomes a carrot or a trumpet, a leg thins toward
// the ankle. This is Blender's "Simple Deform → Taper" and the quickest way to give straight geometry a swelling
// or narrowing profile without remodelling. The axis coordinate of each vertex is untouched; only its distance
// from the axis (through `centre`) is scaled by the ramped factor. axis 0=X, 1=Y, 2=Z. Reuses the same axis
// mapping as the twist deformer. Header-only, pure CPU, deterministic.
//
// Scope note (honest): positions are exact; normals are updated with the inverse scale on their perpendicular
// components and renormalized, which is correct for the cross-section squeeze but ignores the extra tilt the
// taper SLOPE introduces along the axis — for pixel-accurate shading on a strong taper, re-run `computeNormals`
// afterwards. Leaving `axisMin`/`axisMax` at their defaults (min ≥ max) auto-fits the ramp to the mesh's extent
// along the axis. A factor of 0 collapses that end onto the axis line (a true cone tip / degenerate ring — weld
// or reindex if you need the tip to be a single vertex). Factors may exceed 1 (fan outward) or be negative.
namespace maz::render {

// Taper `mesh` along `axis`: the perpendicular cross-section is scaled by lerp(startScale, endScale, t) where t
// runs 0→1 from `axisMin` to `axisMax` along the axis (auto-fit to the mesh extent if axisMin ≥ axisMax). The
// scaling is about the axis line through `centre`.
inline shapes::MeshData taperMesh(const shapes::MeshData& mesh, int axis, float startScale, float endScale,
                                  float axisMin = 1.0f, float axisMax = 0.0f, math::vec3 centre = math::vec3(0, 0, 0)) {
    shapes::MeshData out = mesh;
    if (out.vertices.empty()) return out;
    const int a = axis < 0 ? 0 : (axis > 2 ? 2 : axis);

    auto axisCoord = [a](float x, float y, float z) { return a == 0 ? x : (a == 1 ? y : z); };

    float lo = axisMin, hi = axisMax;
    if (lo >= hi) { // auto-fit the ramp to the mesh's extent along this axis
        lo = axisCoord(out.vertices[0].px, out.vertices[0].py, out.vertices[0].pz);
        hi = lo;
        for (const MeshVertex& v : out.vertices) {
            const float c = axisCoord(v.px, v.py, v.pz);
            if (c < lo) lo = c;
            if (c > hi) hi = c;
        }
    }
    const float span = hi - lo;
    const float invSpan = span > 0.0f ? 1.0f / span : 0.0f;

    // Perpendicular-plane centre offsets (u, v) matching the twist mapping.
    const float cU = a == 0 ? centre.y : (a == 1 ? centre.z : centre.x);
    const float cV = a == 0 ? centre.z : (a == 1 ? centre.x : centre.y);

    for (MeshVertex& vtx : out.vertices) {
        const float coord = axisCoord(vtx.px, vtx.py, vtx.pz);
        float t = (coord - lo) * invSpan;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float scale = startScale + (endScale - startScale) * t;

        // Read this vertex's (u, v) perpendicular position components relative to the centre, and its normal's.
        const float pu = (a == 0 ? vtx.py : (a == 1 ? vtx.pz : vtx.px)) - cU;
        const float pv = (a == 0 ? vtx.pz : (a == 1 ? vtx.px : vtx.py)) - cV;
        const float nu = a == 0 ? vtx.ny : (a == 1 ? vtx.nz : vtx.nx);
        const float nv = a == 0 ? vtx.nz : (a == 1 ? vtx.nx : vtx.ny);

        const float pu2 = pu * scale, pv2 = pv * scale;
        // Inverse scale on the normal's perpendicular components (renormalized below); skip near a collapse.
        const float invS = std::fabs(scale) > 1e-6f ? 1.0f / scale : 1.0f;
        float nu2 = nu * invS, nv2 = nv * invS;
        const float nAxis = a == 0 ? vtx.nx : (a == 1 ? vtx.ny : vtx.nz);
        const float nlen = std::sqrt(nAxis * nAxis + nu2 * nu2 + nv2 * nv2);
        if (nlen > 1e-12f) { nu2 /= nlen; nv2 /= nlen; }
        const float nAxisN = nlen > 1e-12f ? nAxis / nlen : nAxis;

        if (a == 0) {
            vtx.py = pu2 + cU; vtx.pz = pv2 + cV; vtx.nx = nAxisN; vtx.ny = nu2; vtx.nz = nv2;
        } else if (a == 1) {
            vtx.pz = pu2 + cU; vtx.px = pv2 + cV; vtx.ny = nAxisN; vtx.nz = nu2; vtx.nx = nv2;
        } else {
            vtx.px = pu2 + cU; vtx.py = pv2 + cV; vtx.nz = nAxisN; vtx.nx = nu2; vtx.ny = nv2;
        }
    }
    return out;
}

} // namespace maz::render
