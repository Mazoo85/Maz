#pragma once

#include "maz/render/MeshBoundaryLoops.hpp" // extractBoundaryLoops (M538)
#include "maz/render/MeshComponents.hpp"    // connectedComponentLabels (M529)
#include "maz/render/MeshTopology.hpp"      // buildTopology, MeshTopology (M528)
#include "maz/render/Shapes.hpp"            // shapes::MeshData

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

// maz::render TOPOLOGY SUMMARY — one struct answering "what SHAPE, topologically, is this mesh?": how many
// separate pieces (connected components), how many holes ring it (boundary loops), whether it is a closed solid
// (watertight) and manifold, its Euler characteristic V−E+F, and its GENUS — the number of "handles"/through-
// holes (a sphere/box is genus 0, a donut/torus or a coffee mug is genus 1, a pretzel higher). This is the
// mesh-health / "is this printable, is this a valid solid, how complex is it" report a DCC tool or a 3D-print
// slicer shows, and the sanity check before physics/booleans/simplification that assume a clean manifold. It
// composes the earlier connectivity work: MeshTopology (M528) for edges/manifoldness, MeshComponents (M529)
// for the piece count, MeshBoundaryLoops (M538) for the hole count — then genus falls out of the
// Euler–Poincaré formula χ = 2c − 2g − b (c = components, g = genus, b = boundary loops). Header-only, std-only.
//
// Scope note (honest): genus is exact only for a WELDED, ORIENTABLE, manifold mesh (a shared corner must be one
// vertex — run MeshCleanup/MeshWeld first). The vertex count used is the number of REFERENCED vertices (unused
// vertices in the buffer are ignored, so they do not corrupt Euler), but duplicated/un-welded corners still
// would — reported via `manifold`. When the mesh is non-manifold, `genus` is left at -1 (undefined).
namespace maz::render {

struct TopologySummary {
    std::uint32_t vertexCount = 0;         // REFERENCED vertices (distinct indices actually used)
    std::uint32_t edgeCount = 0;           // distinct undirected edges
    std::uint32_t triangleCount = 0;       // faces
    std::uint32_t componentCount = 0;      // connected (edge-linked) pieces
    std::uint32_t boundaryLoopCount = 0;   // hole / open-rim loops
    std::uint32_t boundaryEdgeCount = 0;
    std::uint32_t nonManifoldEdgeCount = 0;
    int eulerCharacteristic = 0;           // V − E + F over referenced vertices
    int genus = -1;                        // handles; -1 if undefined (non-manifold)
    bool closed = false;                   // watertight: no boundary, no non-manifold edges
    bool manifold = false;                 // no edge shared by 3+ triangles
};

// Summarize the topology of `mesh`.
inline TopologySummary summarizeTopology(const shapes::MeshData& mesh) {
    TopologySummary s;
    const MeshTopology topo = buildTopology(mesh);
    s.triangleCount = topo.triangleCount;
    s.edgeCount = topo.edgeCount;
    s.boundaryEdgeCount = topo.boundaryEdgeCount;
    s.nonManifoldEdgeCount = topo.nonManifoldEdgeCount;
    s.manifold = (topo.nonManifoldEdgeCount == 0);
    s.closed = topo.watertight();
    if (topo.triangleCount == 0) return s;

    // Referenced vertex count (ignore unused vertices so Euler stays correct).
    std::unordered_set<std::uint32_t> refd;
    refd.reserve(mesh.indices.size());
    for (std::uint32_t i : mesh.indices) refd.insert(i);
    s.vertexCount = static_cast<std::uint32_t>(refd.size());

    std::uint32_t comps = 0;
    connectedComponentLabels(mesh, comps);
    s.componentCount = comps;

    s.boundaryLoopCount = static_cast<std::uint32_t>(extractBoundaryLoops(mesh).size());

    s.eulerCharacteristic = static_cast<int>(s.vertexCount) - static_cast<int>(s.edgeCount)
                            + static_cast<int>(s.triangleCount);

    // Euler–Poincaré: chi = 2c - 2g - b  =>  g = (2c - b - chi) / 2, valid for an orientable manifold.
    if (s.manifold) {
        const int twoG = 2 * static_cast<int>(s.componentCount) - static_cast<int>(s.boundaryLoopCount)
                         - s.eulerCharacteristic;
        s.genus = (twoG >= 0 && (twoG % 2) == 0) ? twoG / 2 : -1;
    }
    return s;
}

} // namespace maz::render
