#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::render SILHOUETTE / OUTLINE EDGES — find the edges that form a model's OUTLINE as seen from a particular
// direction: the crisp boundary between the parts of the surface facing the camera and the parts facing away. On a
// sphere seen from the front that's the circle around its rim; on a cube seen corner-on it's a hexagon. Unlike the
// engine's sharp/hard/feature-edge tools (M548/M549/M567), which mark folds baked into the geometry no matter where
// you look, a silhouette is VIEW-DEPENDENT — it slides around the surface as the camera moves. It's the thing you
// need for cartoon / ink outlines (draw a fat line along the silhouette), hidden-line and blueprint looks, pencil
// shading, and building shadow-volume "caps" for stencil shadows. An edge is on the silhouette when one of the two
// triangles sharing it faces TOWARD the view and the other faces AWAY; an open boundary edge (only one triangle) is
// always on the outline. Whether the mesh's normals point in or out doesn't matter — flipping them all swaps
// which side is "front" but leaves the front-vs-back BOUNDARY exactly where it was. Header-only, pure CPU.
//
// Scope note (honest): faces are tested by their flat geometric normal (from the winding), so the mesh should be
// consistently wound. A triangle seen exactly edge-on (its normal perpendicular to the view) counts as facing
// away — a deliberate tie-break, only reachable when a face is precisely side-on to the view. `silhouetteEdges`
// takes a single view DIRECTION (orthographic / distant camera); `silhouetteEdgesFromEye` uses a camera POSITION,
// testing each face against its own direction to the eye (correct for a near, perspective camera). Both return the
// silhouette edges as undirected vertex-index pairs; `boundaryCount` says how many of them are open-boundary edges.
namespace maz::render {

struct SilhouetteResult {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges; // undirected silhouette edges (vertex index pairs)
    std::size_t boundaryCount = 0;                             // how many of `edges` are open-boundary edges
};

namespace detail {

// Flat geometric normal of triangle t (not normalized — only its sign vs the view matters).
inline math::vec3 faceNormalOf(const shapes::MeshData& m, std::size_t t) {
    const MeshVertex& a = m.vertices[m.indices[t * 3 + 0]];
    const MeshVertex& b = m.vertices[m.indices[t * 3 + 1]];
    const MeshVertex& c = m.vertices[m.indices[t * 3 + 2]];
    const math::vec3 pa(a.px, a.py, a.pz), pb(b.px, b.py, b.pz), pc(c.px, c.py, c.pz);
    return math::cross(pb - pa, pc - pa);
}

inline math::vec3 faceCentroidOf(const shapes::MeshData& m, std::size_t t) {
    const MeshVertex& a = m.vertices[m.indices[t * 3 + 0]];
    const MeshVertex& b = m.vertices[m.indices[t * 3 + 1]];
    const MeshVertex& c = m.vertices[m.indices[t * 3 + 2]];
    return math::vec3((a.px + b.px + c.px) / 3.0f, (a.py + b.py + c.py) / 3.0f, (a.pz + b.pz + c.pz) / 3.0f);
}

// Shared core: `frontFacing(t)` says whether triangle t faces the view. Returns the silhouette edge set.
template <class FrontFn>
inline SilhouetteResult silhouetteCore(const shapes::MeshData& mesh, FrontFn frontFacing, bool includeBoundary) {
    SilhouetteResult out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0) return out;

    // Per-triangle front/back, then walk each undirected edge collecting (front,back) counts.
    std::vector<std::uint8_t> front(triN);
    for (std::size_t t = 0; t < triN; ++t) front[t] = frontFacing(t) ? 1u : 0u;

    struct EdgeAcc { std::uint32_t a, b; int nFront, nBack; };
    std::unordered_map<std::uint64_t, EdgeAcc> edges;
    edges.reserve(mesh.indices.size());
    for (std::size_t t = 0; t < triN; ++t) {
        for (int e = 0; e < 3; ++e) {
            std::uint32_t v0 = mesh.indices[t * 3 + static_cast<std::size_t>(e)];
            std::uint32_t v1 = mesh.indices[t * 3 + static_cast<std::size_t>((e + 1) % 3)];
            const std::uint32_t lo = v0 < v1 ? v0 : v1, hi = v0 < v1 ? v1 : v0;
            const std::uint64_t key = (static_cast<std::uint64_t>(lo) << 32) | hi;
            auto it = edges.find(key);
            if (it == edges.end()) it = edges.emplace(key, EdgeAcc{lo, hi, 0, 0}).first;
            if (front[t]) ++it->second.nFront; else ++it->second.nBack;
        }
    }

    for (const auto& kv : edges) {
        const EdgeAcc& ea = kv.second;
        const int total = ea.nFront + ea.nBack;
        if (total == 1) { // open boundary edge — always on the outline
            if (includeBoundary) { out.edges.emplace_back(ea.a, ea.b); ++out.boundaryCount; }
        } else if (ea.nFront > 0 && ea.nBack > 0) { // front on one side, back on the other -> silhouette
            out.edges.emplace_back(ea.a, ea.b);
        }
    }
    return out;
}

} // namespace detail

// Orthographic silhouette: `viewDir` is the direction the camera looks (into the scene).
inline SilhouetteResult silhouetteEdges(const shapes::MeshData& mesh, const math::vec3& viewDir,
                                        bool includeBoundary = true) {
    return detail::silhouetteCore(
        mesh, [&](std::size_t t) { return math::dot(detail::faceNormalOf(mesh, t), viewDir) < 0.0f; },
        includeBoundary);
}

// Perspective silhouette: `eye` is the camera position; each face is tested against its own direction to the eye.
inline SilhouetteResult silhouetteEdgesFromEye(const shapes::MeshData& mesh, const math::vec3& eye,
                                               bool includeBoundary = true) {
    return detail::silhouetteCore(
        mesh,
        [&](std::size_t t) {
            const math::vec3 toFace = detail::faceCentroidOf(mesh, t) - eye; // eye -> face = view direction there
            return math::dot(detail::faceNormalOf(mesh, t), toFace) < 0.0f;
        },
        includeBoundary);
}

} // namespace maz::render
