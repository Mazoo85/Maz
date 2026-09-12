#pragma once

#include "maz/math/Math.hpp"        // math::vec3
#include "maz/render/MeshTools.hpp"  // computeNormals
#include "maz/render/Shapes.hpp"     // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render SKIN / LOFT ACROSS SECTIONS — stretch a smooth surface over a stack of cross-section "ribs", like
// pulling a skin over the frames of a boat hull or an aeroplane fuselage. You give it an ordered list of rings
// (each ring is a loop of 3D points — the outline of the shape at that station), and it bridges every rib to the
// next with a band of triangles, so the shape flows from one outline into the next. Unlike sweep (which drags ONE
// fixed profile along a path), each rib here can be a DIFFERENT size and shape, so the surface can taper, bulge,
// twist, or morph: a funnel (big ring → small ring), a boat hull (keel → beam → stern), a vase whose silhouette
// changes freely, a tube that fairs from a circle into a square. This is Blender's "Bridge Edge Loops" / classic
// CAD lofting. Reuses the engine's area-weighted `computeNormals` for smooth shading. Header-only, deterministic.
//
// Scope note (honest): every rib must have the SAME number of points (point j of one rib connects to point j of
// the next — there is no resampling), and the ribs should be given in order along the body. `closedRings` (default
// true) picks whether each rib is a closed loop (a tube) or an open strip; `closedPath` (default false) also
// bridges the last rib back to the first for a closed torus-like body. This builds the swept SIDE skin only — the
// two end ribs are left OPEN (cap them separately for a closed solid). Needs >= 2 ribs, each with >= 2 points.
namespace maz::render {

// Loft a skin across `sections` (ordered ribs, each an equal-length ring of 3D points). Returns the skin mesh.
inline shapes::MeshData skinSections(const std::vector<std::vector<math::vec3>>& sections,
                                     bool closedRings = true, bool closedPath = false) {
    shapes::MeshData out;
    const std::size_t S = sections.size();
    if (S < 2) return out;
    const std::size_t P = sections[0].size();
    if (P < 2) return out;
    for (const std::vector<math::vec3>& rib : sections)
        if (rib.size() != P) return out; // ragged ribs are rejected (no resampling)

    // --- Positions: every rib laid down in order. ---
    std::vector<math::vec3> positions;
    positions.reserve(S * P);
    for (std::size_t i = 0; i < S; ++i)
        for (std::size_t j = 0; j < P; ++j) positions.push_back(sections[i][j]);

    // --- Faces: bridge rib i to rib i+1 (and optionally the last back to the first). ---
    const std::size_t edges = closedRings ? P : P - 1;
    const std::size_t bridges = closedPath ? S : S - 1; // number of rib-to-rib bands
    std::vector<std::uint32_t> indices;
    indices.reserve(bridges * edges * 6);
    for (std::size_t bi = 0; bi < bridges; ++bi) {
        const std::size_t i0 = bi;
        const std::size_t i1 = (bi + 1) % S;
        for (std::size_t e = 0; e < edges; ++e) {
            const std::size_t j0 = e, j1 = (e + 1) % P;
            const std::uint32_t a = static_cast<std::uint32_t>(i0 * P + j0);
            const std::uint32_t b = static_cast<std::uint32_t>(i0 * P + j1);
            const std::uint32_t c = static_cast<std::uint32_t>(i1 * P + j0);
            const std::uint32_t d = static_cast<std::uint32_t>(i1 * P + j1);
            indices.push_back(a); indices.push_back(c); indices.push_back(d);
            indices.push_back(a); indices.push_back(d); indices.push_back(b);
        }
    }

    // --- Smooth normals + assemble vertices (white, UV = [rib index, around ring]). ---
    const std::vector<math::vec3> normals = computeNormals(positions, indices);
    out.vertices.reserve(positions.size());
    for (std::size_t i = 0; i < S; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(S - 1);
        for (std::size_t j = 0; j < P; ++j) {
            const std::size_t k = i * P + j;
            MeshVertex v{};
            v.px = positions[k].x; v.py = positions[k].y; v.pz = positions[k].z;
            v.nx = normals[k].x; v.ny = normals[k].y; v.nz = normals[k].z;
            v.r = v.g = v.b = 1.0f;
            v.u = u;
            v.v = static_cast<float>(j) / static_cast<float>(P);
            out.vertices.push_back(v);
        }
    }
    out.indices = std::move(indices);
    return out;
}

} // namespace maz::render
