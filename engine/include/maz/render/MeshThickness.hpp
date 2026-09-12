#pragma once

#include "maz/math/Math.hpp"                     // math::vec3, cross, dot
#include "maz/render/MeshAmbientOcclusion.hpp"   // detail::aoRayTri (Möller–Trumbore)
#include "maz/render/Shapes.hpp"                 // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// maz::render MESH WALL-THICKNESS PROBE — measure how THICK the material is at every point of a surface by
// shooting a ray straight INTO the surface (opposite its outward normal) and returning the distance to the wall
// on the far side. This is the "wall thickness" / "shell gauge" check 3D-printing slicers and CAD tools run to
// catch spots too thin to print or too thin to be structurally sound; it also drives subsurface-scattering
// thickness maps (skin, wax, leaves glowing at their thin edges) and "is this hollow shell uniform?" audits.
// For a solid model the probe reports the full span across the object; for a hollow shell it reports the gap
// between the outer and inner walls. Reuses the M533 ambient-occlusion ray/triangle test; header-only, pure CPU.
//
// Scope note (honest): this is a single inward ray per vertex along the smooth normal — it measures thickness in
// exactly that direction, so a wall sampled at a glancing angle reads thicker than its true minimum; average
// several offset rays if you need a robust minimum. A vertex whose inward ray escapes without hitting anything
// within `maxDistance` (an open edge, a one-sided sheet) is reported as `maxDistance` and flagged "open". Normals
// are area-weighted smooth normals recomputed internally, so winding must be outward-consistent for the ray to
// point into the material.
namespace maz::render {

// Per-vertex wall thickness: distance from each vertex, travelling inward along −normal, to the first triangle
// on the far wall. Vertices with no opposite wall within `maxDistance` are set to `maxDistance` (treat as open).
inline std::vector<float> computeThickness(const shapes::MeshData& mesh, float maxDistance = 1e6f) {
    const std::size_t vn = mesh.vertices.size();
    std::vector<float> thick(vn, maxDistance);
    if (mesh.indices.size() < 3 || vn == 0 || maxDistance <= 0.0f) return thick;

    std::vector<math::vec3> pos(vn), nrm(vn, math::vec3(0, 0, 0));
    for (std::size_t i = 0; i < vn; ++i) {
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
    }
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t ia = mesh.indices[t], ib = mesh.indices[t + 1], ic = mesh.indices[t + 2];
        if (ia >= vn || ib >= vn || ic >= vn) continue;
        const math::vec3 fn = math::cross(pos[ib] - pos[ia], pos[ic] - pos[ia]); // area-weighted, unnormalized
        nrm[ia] += fn;
        nrm[ib] += fn;
        nrm[ic] += fn;
    }
    for (math::vec3& n : nrm) {
        const float l = std::sqrt(math::dot(n, n));
        n = l > 1e-12f ? n / l : math::vec3(0, 1, 0);
    }

    for (std::size_t vi = 0; vi < vn; ++vi) {
        const math::vec3 dir = math::vec3(-nrm[vi].x, -nrm[vi].y, -nrm[vi].z); // shoot into the material
        const math::vec3 origin = pos[vi] + dir * 1e-3f;                       // step inward to dodge self-hit
        float best = maxDistance;
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const std::uint32_t ia = mesh.indices[t], ib = mesh.indices[t + 1], ic = mesh.indices[t + 2];
            if (ia == vi || ib == vi || ic == vi) continue; // never measure against the vertex's own faces
            float tt;
            if (detail::aoRayTri(origin, dir, pos[ia], pos[ib], pos[ic], best, tt)) {
                if (tt < best) best = tt;
            }
        }
        thick[vi] = best;
    }
    return thick;
}

// Summary gauge over a whole mesh. `openVertices` (if provided) receives the count of vertices with no opposing
// wall within `maxDistance`. Returns the smallest FINITE thickness found (the thinnest wall) and the mean of the
// finite thicknesses; both are `maxDistance` when nothing measurable was found.
struct ThicknessReport {
    float minThickness = 0.0f;  // thinnest wall (the print/structural risk spot)
    float meanThickness = 0.0f; // average finite wall thickness
    std::size_t measured = 0;   // vertices that hit an opposite wall
    std::size_t open = 0;       // vertices whose inward ray escaped (open edges / one-sided sheets)
};

inline ThicknessReport analyzeThickness(const shapes::MeshData& mesh, float maxDistance = 1e6f) {
    ThicknessReport rep;
    const std::vector<float> thick = computeThickness(mesh, maxDistance);
    float mn = std::numeric_limits<float>::max();
    double sum = 0.0;
    for (float d : thick) {
        if (d < maxDistance) {
            mn = d < mn ? d : mn;
            sum += static_cast<double>(d);
            ++rep.measured;
        } else {
            ++rep.open;
        }
    }
    if (rep.measured > 0) {
        rep.minThickness = mn;
        rep.meanThickness = static_cast<float>(sum / static_cast<double>(rep.measured));
    } else {
        rep.minThickness = maxDistance;
        rep.meanThickness = maxDistance;
    }
    return rep;
}

} // namespace maz::render
