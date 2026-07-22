#pragma once

#include "maz/math/Math.hpp"                 // math::vec3, dot
#include "maz/render/MeshMassProperties.hpp" // computeMassProperties (M532)
#include "maz/render/Shapes.hpp"             // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::render PIVOT SNAP / RECENTER — move a mesh's PIVOT (the point that ends up at the world origin) to a
// sensible place. Imported models land wherever the exporter left them — floating off-axis, pivot in a random
// corner — which makes them awkward to place, rotate, and scale in a scene. This recentres the geometry so its
// pivot sits at the origin, choosing the pivot by intent: the BOUNDING-BOX CENTRE (spin-in-place props), the
// BASE (characters, trees, furniture that stand on the ground — bottom-centre), the CENTRE OF MASS (physics
// bodies that should rotate about their true balance point), or the VERTEX AVERAGE (a cheap centroid). It returns
// a recentred copy plus the applied offset and the world-space pivot, so callers can compensate a parent
// transform. Reuses the M532 mass-properties centroid for the centre-of-mass mode. Header-only, pure CPU.
//
// Scope note (honest): this only TRANSLATES — it never rotates or scales (pair with M568 dominant-plane to also
// align, or a normalize-to-box pass to also scale). CENTRE OF MASS needs a closed, consistently-wound solid
// (M566/M562); on an open or degenerate mesh it falls back to the vertex average and sets `fellBack`. BASE uses
// the supplied `up` axis (default +Y): the pivot is the bbox centre in the two perpendicular axes and the minimum
// extent along `up`. Only vertex POSITIONS move; normals/UVs/colours are untouched.
namespace maz::render {

enum class PivotMode : std::uint8_t { BBoxCenter = 0, Base = 1, CenterOfMass = 2, VertexAverage = 3 };

struct RecenterResult {
    shapes::MeshData mesh;         // recentred copy (pivot moved to the origin)
    math::vec3 offset{0, 0, 0};    // translation applied to every vertex (newPos = oldPos + offset)
    math::vec3 pivot{0, 0, 0};     // world-space pivot that was moved to the origin (== −offset)
    bool fellBack = false;         // CenterOfMass requested but the mesh wasn't a closed solid -> used average
};

// Recentre `mesh` so its chosen pivot sits at the origin. `up` (for Base mode) defaults to +Y.
inline RecenterResult recenterMesh(const shapes::MeshData& mesh, PivotMode mode = PivotMode::BBoxCenter,
                                   math::vec3 up = math::vec3(0, 1, 0)) {
    RecenterResult out;
    out.mesh = mesh;
    const std::size_t n = mesh.vertices.size();
    if (n == 0) return out;

    math::vec3 lo, hi, sum(0, 0, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        const math::vec3 p(v.px, v.py, v.pz);
        sum += p;
        if (i == 0) { lo = hi = p; }
        else {
            lo = math::vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
            hi = math::vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
        }
    }
    const math::vec3 bboxCentre = (lo + hi) * 0.5f;
    const math::vec3 average = sum * (1.0f / static_cast<float>(n));

    math::vec3 pivot = bboxCentre;
    switch (mode) {
        case PivotMode::BBoxCenter:
            pivot = bboxCentre;
            break;
        case PivotMode::VertexAverage:
            pivot = average;
            break;
        case PivotMode::CenterOfMass: {
            const MassProperties mp = computeMassProperties(mesh);
            if (mp.valid) {
                pivot = math::vec3(static_cast<float>(mp.centroid[0]), static_cast<float>(mp.centroid[1]),
                                   static_cast<float>(mp.centroid[2]));
            } else {
                pivot = average; // open/degenerate mesh: no true centre of mass
                out.fellBack = true;
            }
            break;
        }
        case PivotMode::Base: {
            const float ul = std::sqrt(math::dot(up, up));
            const math::vec3 u = ul > 1e-12f ? math::vec3(up.x / ul, up.y / ul, up.z / ul) : math::vec3(0, 1, 0);
            float minAlongUp = 1e30f;
            for (std::size_t i = 0; i < n; ++i) {
                const MeshVertex& v = mesh.vertices[i];
                minAlongUp = std::min(minAlongUp, math::dot(math::vec3(v.px, v.py, v.pz), u));
            }
            // bbox centre, but slid down the up axis to the lowest extent (bottom-centre).
            const float shift = minAlongUp - math::dot(bboxCentre, u);
            pivot = bboxCentre + u * shift;
            break;
        }
    }

    out.pivot = pivot;
    out.offset = math::vec3(-pivot.x, -pivot.y, -pivot.z);
    for (MeshVertex& v : out.mesh.vertices) {
        v.px += out.offset.x;
        v.py += out.offset.y;
        v.pz += out.offset.z;
    }
    return out;
}

} // namespace maz::render
