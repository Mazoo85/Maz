#pragma once

#include "maz/render/MeshBoundaryLoops.hpp" // extractBoundaryLoops (M536)
#include "maz/render/Shapes.hpp"            // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render HOLE FILL / CAP — seal the open holes a mesh has, turning a leaky surface into a watertight solid.
// Where M566 REPORTS holes, this PATCHES them: for each open boundary loop it adds a centre vertex at the hole's
// average position and fans triangles from that centre to the rim, closing the gap. That's the "fill holes /
// make watertight" repair 3D-print prep, scan cleanup, and boolean/CSG post-processing all run so the mesh
// passes solid checks (volume, mass, inside/outside, printing). The rim comes from the M536 boundary-loop walk,
// whose ordering follows the existing faces' winding, so each cap triangle is wound to MATCH its neighbours (no
// flipped patch). A `maxEdges` limit fills only small holes (pinholes and cut faces) while leaving big openings —
// a deliberately open cup mouth, say — untouched. Header-only, pure CPU.
//
// Scope note (honest): this is a simple centre-fan cap — ideal for small, roughly-flat or convex holes (cut
// faces, pinholes); a large or highly non-planar hole gets a valid but crude flat-ish patch (feed it to M540/
// M564 smoothing or a remesh for a nicer surface). The added centre vertex copies the rim's average colour; its
// normal is left zero for a downstream computeNormals pass. Non-manifold edges are not repaired (a distinct
// problem). After filling, the mesh is edge-watertight over the capped loops (verify with M566).
namespace maz::render {

struct HoleFillResult {
    shapes::MeshData mesh;          // copy of the input with holes capped
    std::size_t holesFilled = 0;    // boundary loops that were sealed
    std::size_t trianglesAdded = 0; // cap triangles added in total
};

// Cap the holes of `mesh`. `maxEdges` > 0 fills only holes whose rim has at most that many edges (0 = fill all).
inline HoleFillResult fillHoles(const shapes::MeshData& mesh, std::size_t maxEdges = 0) {
    HoleFillResult out;
    out.mesh = mesh;
    const std::vector<std::vector<std::uint32_t>> loops = extractBoundaryLoops(mesh);
    if (loops.empty()) return out;

    for (const std::vector<std::uint32_t>& loop : loops) {
        const std::size_t n = loop.size();
        if (n < 3) continue;
        if (maxEdges > 0 && n > maxEdges) continue; // skip holes bigger than the limit

        // Centre vertex = average of the rim (position + colour); normal left zero for a later normal pass.
        MeshVertex centre{};
        for (std::uint32_t vi : loop) {
            const MeshVertex& v = mesh.vertices[vi];
            centre.px += v.px;
            centre.py += v.py;
            centre.pz += v.pz;
            centre.r += v.r;
            centre.g += v.g;
            centre.b += v.b;
        }
        const float inv = 1.0f / static_cast<float>(n);
        centre.px *= inv;
        centre.py *= inv;
        centre.pz *= inv;
        centre.r *= inv;
        centre.g *= inv;
        centre.b *= inv;
        const std::uint32_t c = static_cast<std::uint32_t>(out.mesh.vertices.size());
        out.mesh.vertices.push_back(centre);

        // The rim is ordered along the existing faces' boundary direction (rim[i] -> rim[i+1]); cap the edge in
        // the OPPOSITE direction (centre, rim[i+1], rim[i]) so the patch normal matches its neighbours.
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t a = loop[i];
            const std::uint32_t b = loop[(i + 1) % n];
            out.mesh.indices.push_back(c);
            out.mesh.indices.push_back(b);
            out.mesh.indices.push_back(a);
            ++out.trianglesAdded;
        }
        ++out.holesFilled;
    }
    return out;
}

} // namespace maz::render
