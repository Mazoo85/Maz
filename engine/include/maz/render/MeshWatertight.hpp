#pragma once

#include "maz/math/Math.hpp"                  // math::vec3, dot
#include "maz/render/MeshBoundaryLoops.hpp"   // extractBoundaryLoops (M536)
#include "maz/render/MeshTopology.hpp"        // buildTopology, MeshTopology (M528)
#include "maz/render/Shapes.hpp"              // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render WATERTIGHTNESS / HOLE REPORT — is this mesh SEALED, or does it have gaps? A watertight (closed)
// surface has no open edges and no edge shared by three-plus faces; it's what 3D printing, boolean/CSG, solid
// physics, volume/mass, and inside/outside tests all require. This walks the mesh's edges to answer "is it
// closed?" and, when it isn't, finds every HOLE — each open boundary loop — reporting how many there are and, per
// hole, its rim as an ordered vertex loop with an edge count and perimeter length (so you can rank the big gaps
// worth patching from the pinholes). It also surfaces NON-MANIFOLD edges (three-plus faces meeting), the other
// way a mesh fails to be a clean solid. Reuses the M528 half-edge topology and the M536 boundary-loop extractor.
// Header-only, pure CPU — the analysis behind an editor's "mesh is not watertight: N holes" warning.
//
// Scope note (honest): "watertight" here is EDGE-manifold closure (no boundary, no 3+-face edges) — the standard
// printability/solidity test; it does not separately verify consistent winding (use M562) or self-intersection
// (a distinct, costlier check). Holes are open boundary loops, so a mesh split into separate closed shells reads
// as watertight with zero holes even though it is several pieces (pair with M529 components if that matters).
namespace maz::render {

struct MeshHole {
    std::vector<std::uint32_t> loop; // ordered boundary vertex indices around the hole
    std::size_t edgeCount = 0;       // edges around the rim (== loop.size())
    double perimeter = 0.0;          // total rim length
};

struct WatertightReport {
    bool watertight = false;             // no boundary edges and no non-manifold edges
    std::size_t boundaryEdgeCount = 0;   // open edges (used by exactly one triangle)
    std::size_t nonManifoldEdgeCount = 0;// edges shared by three or more triangles
    std::size_t holeCount = 0;           // number of open boundary loops
    std::vector<MeshHole> holes;         // per-hole detail, largest perimeter first
    double largestHolePerimeter = 0.0;
};

// Analyse whether `mesh` is watertight and enumerate its holes.
inline WatertightReport analyzeWatertight(const shapes::MeshData& mesh) {
    WatertightReport rep;
    const MeshTopology topo = buildTopology(mesh);
    rep.boundaryEdgeCount = topo.boundaryEdgeCount;
    rep.nonManifoldEdgeCount = topo.nonManifoldEdgeCount;
    rep.watertight = topo.watertight();
    if (topo.triangleCount == 0) return rep;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    const std::vector<std::vector<std::uint32_t>> loops = extractBoundaryLoops(mesh);
    rep.holes.reserve(loops.size());
    for (const std::vector<std::uint32_t>& loop : loops) {
        if (loop.size() < 3) continue; // a valid rim needs at least three edges
        MeshHole hole;
        hole.loop = loop;
        hole.edgeCount = loop.size();
        double per = 0.0;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const math::vec3 a = pos(loop[i]);
            const math::vec3 b = pos(loop[(i + 1) % loop.size()]);
            per += std::sqrt(static_cast<double>(math::dot(b - a, b - a)));
        }
        hole.perimeter = per;
        rep.holes.push_back(std::move(hole));
    }
    std::sort(rep.holes.begin(), rep.holes.end(),
              [](const MeshHole& x, const MeshHole& y) { return x.perimeter > y.perimeter; });
    rep.holeCount = rep.holes.size();
    if (!rep.holes.empty()) rep.largestHolePerimeter = rep.holes.front().perimeter;
    return rep;
}

} // namespace maz::render
