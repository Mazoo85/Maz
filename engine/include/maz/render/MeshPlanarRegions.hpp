#pragma once

#include "maz/math/Math.hpp"           // math::vec3, cross, dot, normalize
#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render COPLANAR REGION segmentation — group a mesh's triangles into maximal CONNECTED, near-PLANAR
// patches: the flat faces of a shape. It is the workhorse behind collision-hull simplification (one convex
// face instead of a triangle fan), greedy meshing, decal/lightmap chart seeding, and "select coplanar" in an
// editor. Flood-fill across the shared MeshTopology (M528) edge-neighbours, but only cross an edge when the
// neighbour triangle's face normal stays within `angleTolerance` of the region's SEED normal — so every
// triangle in a region is guaranteed planar to the seed within that tolerance (not just to its neighbour,
// which would let a region slowly bend away). A cube yields exactly six regions, a curved surface yields
// many. Pure CPU, header-only, headless.
namespace maz::render {

struct CoplanarRegions {
    std::uint32_t count = 0;
    std::vector<std::uint32_t> triLabel;      // per-triangle region id in [0, count)
    std::vector<math::vec3> regionNormal;     // the seed face normal of each region (zero for degenerate)
};

namespace detail {
inline math::vec3 faceNormal(const shapes::MeshData& m, std::uint32_t tri) {
    const MeshVertex& a = m.vertices[m.indices[static_cast<std::size_t>(tri) * 3]];
    const MeshVertex& b = m.vertices[m.indices[static_cast<std::size_t>(tri) * 3 + 1]];
    const MeshVertex& c = m.vertices[m.indices[static_cast<std::size_t>(tri) * 3 + 2]];
    const math::vec3 n = math::cross(math::vec3(b.px - a.px, b.py - a.py, b.pz - a.pz),
                                     math::vec3(c.px - a.px, c.py - a.py, c.pz - a.pz));
    const float len = std::sqrt(math::dot(n, n));
    return len > 1e-12f ? n / len : math::vec3(0, 0, 0);
}
} // namespace detail

// Segment `mesh` into connected coplanar regions. Two triangles join a region when they are edge-connected
// and the joiner's face normal is within `angleToleranceDegrees` of the region's seed normal. Regions are
// numbered in order of first-seen triangle (deterministic). Degenerate (zero-area) triangles seed their own
// singleton regions.
inline CoplanarRegions segmentCoplanarRegions(const shapes::MeshData& mesh, float angleToleranceDegrees) {
    CoplanarRegions out;
    const MeshTopology topo = buildTopology(mesh);
    const std::uint32_t triCount = topo.triangleCount;
    out.triLabel.assign(triCount, MeshTopology::kNone);
    if (triCount == 0) return out;

    const float cosTol = std::cos(angleToleranceDegrees * 3.14159265358979323846f / 180.0f);
    std::vector<std::uint32_t> stack;
    for (std::uint32_t seed = 0; seed < triCount; ++seed) {
        if (out.triLabel[seed] != MeshTopology::kNone) continue;
        const std::uint32_t region = out.count++;
        const math::vec3 seedN = detail::faceNormal(mesh, seed);
        out.regionNormal.push_back(seedN);
        out.triLabel[seed] = region;
        stack.push_back(seed);
        while (!stack.empty()) {
            const std::uint32_t t = stack.back();
            stack.pop_back();
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t nb = topo.triangleNeighbor(t, e);
                if (nb == MeshTopology::kNone || out.triLabel[nb] != MeshTopology::kNone) continue;
                const math::vec3 nbN = detail::faceNormal(mesh, nb);
                if (math::dot(nbN, seedN) >= cosTol) { // within tolerance of the SEED plane
                    out.triLabel[nb] = region;
                    stack.push_back(nb);
                }
            }
        }
    }
    return out;
}

} // namespace maz::render
