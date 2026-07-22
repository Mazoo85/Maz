#pragma once

#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render CONNECTED COMPONENTS / mesh island splitting — separate a triangle soup into the independent
// sub-meshes that are actually STITCHED together, the "Mesh > Separate / by loose parts" operation every DCC
// and Godot's own tooling offers. Two triangles belong to the same island when they share an EDGE (a
// vertex-only touch does NOT connect them, matching how importers and physics treat loose parts). Built on
// the shared MeshTopology (M528): flood-fill triangles across their edge-twins, then compact each island into
// its own MeshData with a remapped, minimal vertex list. Useful for per-part physics bodies, per-island
// culling/streaming, cleaning stray geometry, and splitting a merged export back into pieces. Pure CPU,
// header-only, headless.
namespace maz::render {

// Per-triangle island label in [0, componentCount). `componentCount` is returned via the out-param. Labels
// are assigned in order of first-seen triangle, so the result is deterministic.
inline std::vector<std::uint32_t> connectedComponentLabels(const shapes::MeshData& mesh,
                                                           std::uint32_t& componentCount) {
    const MeshTopology topo = buildTopology(mesh);
    const std::uint32_t triCount = topo.triangleCount;
    std::vector<std::uint32_t> label(triCount, MeshTopology::kNone);
    componentCount = 0;
    std::vector<std::uint32_t> stack;
    for (std::uint32_t seed = 0; seed < triCount; ++seed) {
        if (label[seed] != MeshTopology::kNone) continue;
        const std::uint32_t comp = componentCount++;
        label[seed] = comp;
        stack.push_back(seed);
        while (!stack.empty()) {
            const std::uint32_t t = stack.back();
            stack.pop_back();
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t nb = topo.triangleNeighbor(t, e);
                if (nb != MeshTopology::kNone && label[nb] == MeshTopology::kNone) {
                    label[nb] = comp;
                    stack.push_back(nb);
                }
            }
        }
    }
    return label;
}

// Split `mesh` into its edge-connected islands. Each returned MeshData carries only the vertices its triangles
// reference (remapped to a compact 0..n range); triangle winding and vertex attributes are preserved. The
// islands are ordered by first-seen triangle (deterministic). An empty / triangle-less mesh yields no
// components.
inline std::vector<shapes::MeshData> splitConnectedComponents(const shapes::MeshData& mesh) {
    std::uint32_t componentCount = 0;
    const std::vector<std::uint32_t> label = connectedComponentLabels(mesh, componentCount);

    std::vector<shapes::MeshData> out(componentCount);
    // Per-component vertex remap: old vertex index -> new index within that component (kNone = not yet added).
    std::vector<std::vector<std::uint32_t>> remap(
        componentCount, std::vector<std::uint32_t>(mesh.vertices.size(), MeshTopology::kNone));

    for (std::uint32_t t = 0; t < label.size(); ++t) {
        const std::uint32_t comp = label[t];
        shapes::MeshData& dst = out[comp];
        std::vector<std::uint32_t>& rm = remap[comp];
        for (int e = 0; e < 3; ++e) {
            const std::uint32_t oldIdx = mesh.indices[static_cast<std::size_t>(t) * 3
                                                      + static_cast<std::uint32_t>(e)];
            std::uint32_t& slot = rm[oldIdx];
            if (slot == MeshTopology::kNone) {
                slot = static_cast<std::uint32_t>(dst.vertices.size());
                dst.vertices.push_back(mesh.vertices[oldIdx]);
            }
            dst.indices.push_back(slot);
        }
    }
    return out;
}

} // namespace maz::render
