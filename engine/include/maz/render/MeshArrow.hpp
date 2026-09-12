#pragma once

#include "maz/math/Math.hpp"        // math::vec3
#include "maz/render/MeshTools.hpp"  // computeNormals
#include "maz/render/Shapes.hpp"     // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render ARROW MESH — a solid 3D arrow (a round shaft with a cone tip) pointing along +Y. Arrows are the
// universal "look here / this way" marker: draw a force or velocity vector, show which way a spawn/waypoint faces,
// build the move/rotate gizmo handles for an editor, point at an objective, make a compass needle or a
// wind-direction indicator. Point it wherever you like by rotating the mesh (aim +Y at your target). It is one
// closed watertight solid — shaft tube + bottom cap + the flat under-shoulder of the head + the cone — so it
// lights and casts shadows like any prop. Reuses the engine's area-weighted `computeNormals`. Header-only,
// deterministic, headless — pure CPU geometry.
//
// Scope note (honest): the total `length` runs from the base at y=0 to the tip at y=length; `headLength` is how
// much of that is the cone (clamped into the open interval so there is always some shaft and some head),
// `shaftRadius` < `headRadius` gives the classic arrow shoulder. Normals are smoothed across the shaft/shoulder/
// cone joins (one `computeNormals` pass over shared vertices), so the creases read a touch soft — split the
// vertices for razor-sharp edges if you need them. `segments` (>= 3) sets how round the shaft/cone are. Bad params
// (length <= 0, radii <= 0, segments < 3) return an empty mesh.
namespace maz::render {

// Build a +Y-pointing solid arrow. Base at y=0, tip at y=`length`; `headLength` of the top is the cone.
inline shapes::MeshData buildArrow(float length = 1.0f, float shaftRadius = 0.03f, float headRadius = 0.08f,
                                   float headLength = 0.25f, int segments = 16) {
    shapes::MeshData out;
    if (length <= 0.0f || shaftRadius <= 0.0f || headRadius <= 0.0f || segments < 3) return out;
    // Keep some shaft and some head no matter what was asked.
    float hl = headLength;
    if (hl <= 0.0f) hl = length * 0.25f;
    if (hl >= length) hl = length * 0.999f;
    const float shaftLen = length - hl;
    const std::size_t seg = static_cast<std::size_t>(segments);
    const float twoPi = 6.28318530717958647692f;

    std::vector<math::vec3> p;
    p.reserve(seg * 3 + 2);
    auto ring = [&](float r, float y) {
        for (std::size_t k = 0; k < seg; ++k) {
            const float a = twoPi * static_cast<float>(k) / static_cast<float>(seg);
            p.push_back(math::vec3(std::cos(a) * r, y, std::sin(a) * r));
        }
    };
    ring(shaftRadius, 0.0f);       // ring0: [0, seg)         shaft bottom
    ring(shaftRadius, shaftLen);   // ring1: [seg, 2seg)      shaft top
    ring(headRadius, shaftLen);    // ring2: [2seg, 3seg)     head base (the shoulder)
    const std::uint32_t tip = static_cast<std::uint32_t>(p.size()); p.push_back(math::vec3(0, length, 0));
    const std::uint32_t bottom = static_cast<std::uint32_t>(p.size()); p.push_back(math::vec3(0, 0, 0));

    const std::uint32_t r0 = 0, r1 = static_cast<std::uint32_t>(seg), r2 = static_cast<std::uint32_t>(2 * seg);
    std::vector<std::uint32_t> idx;
    idx.reserve(seg * 3 * 4);
    auto tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) { idx.push_back(a); idx.push_back(b); idx.push_back(c); };
    for (std::size_t k = 0; k < seg; ++k) {
        const std::uint32_t k0 = static_cast<std::uint32_t>(k), k1 = static_cast<std::uint32_t>((k + 1) % seg);
        // Shaft side (outward radial).
        tri(r0 + k0, r1 + k0, r1 + k1);
        tri(r0 + k0, r1 + k1, r0 + k1);
        // Bottom cap (faces -Y): centre -> ring0.
        tri(bottom, r0 + k0, r0 + k1);
        // Head shoulder: flat ring between shaft-top (r1) and head-base (r2), faces -Y (the underside overhang).
        tri(r1 + k0, r2 + k1, r1 + k1);
        tri(r1 + k0, r2 + k0, r2 + k1);
        // Cone side (r2 base -> tip), outward-facing.
        tri(r2 + k0, tip, r2 + k1);
    }

    const std::vector<math::vec3> nrm = computeNormals(p, idx);
    out.vertices.reserve(p.size());
    for (std::size_t i = 0; i < p.size(); ++i) {
        MeshVertex v{};
        v.px = p[i].x; v.py = p[i].y; v.pz = p[i].z;
        v.nx = nrm[i].x; v.ny = nrm[i].y; v.nz = nrm[i].z;
        v.r = v.g = v.b = 1.0f;
        v.u = 0.0f; v.v = p[i].y / length;
        out.vertices.push_back(v);
    }
    out.indices = std::move(idx);
    return out;
}

} // namespace maz::render
