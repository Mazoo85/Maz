#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render UV-SEAM EDGE DETECTION — find the edges of a mesh where the texture coordinates are DISCONTINUOUS:
// the two triangles that meet along a 3D edge disagree on the UV of the shared corners, so the texture is cut
// there. Those cuts are the seams of a UV unwrap — the boundaries of the flat "islands" a model is unfolded into
// (a cube unwrapped as a cross has a seam along most of its rim). Knowing them drives: lightmap / texture-atlas
// SEAM DILATION (bleed colour a few texels past a seam so bilinear filtering doesn't sample the gap), seam-hiding
// and seam-aware smoothing, and the "select seams" convenience in a UV editor. Detection welds vertices by
// POSITION so the same physical edge from two UV islands is recognised as one edge, then compares the UVs the
// two faces assign at each endpoint. Reuses nothing beyond std. Header-only, deterministic.
//
// Scope note (honest): compares the mesh's stored per-vertex UVs, so a mesh that already welds UV-identical
// corners simply reports no seam there (correct). Position welding is grid-quantised by `posEps`; a boundary
// edge (one triangle) is an island rim, reported separately from interior UV-discontinuity seams. Non-manifold
// position-edges (3+ triangles) are counted but not seam-classified.
namespace maz::render {

struct UvSeamResult {
    // One representative endpoint-vertex pair (va, vb) per interior edge flagged as a UV seam.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> seamEdges;
    std::uint32_t interiorEdgeCount = 0;   // position-edges shared by exactly two triangles
    std::uint32_t boundaryEdgeCount = 0;   // position-edges used by exactly one triangle (island rim)
    std::uint32_t nonManifoldEdgeCount = 0;// position-edges shared by three or more triangles
    std::size_t seamCount() const { return seamEdges.size(); }
};

// Detect the UV seams of `mesh`. `posEps` welds coincident positions; `uvEps` is the UV-match tolerance.
inline UvSeamResult detectUvSeams(const shapes::MeshData& mesh, float posEps = 1e-5f, float uvEps = 1e-6f) {
    UvSeamResult out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0 || mesh.vertices.empty()) return out;

    // Canonical position id per vertex (grid-quantised weld).
    const float inv = posEps > 0.0f ? 1.0f / posEps : 1.0f;
    auto pkey = [&](const MeshVertex& v) {
        auto q = [inv](float c) { return static_cast<long long>(std::llround(static_cast<double>(c) * inv)); };
        const std::uint64_t a = static_cast<std::uint64_t>(q(v.px)) * 73856093ULL;
        const std::uint64_t b = static_cast<std::uint64_t>(q(v.py)) * 19349663ULL;
        const std::uint64_t c = static_cast<std::uint64_t>(q(v.pz)) * 83492791ULL;
        return a ^ b ^ c;
    };
    std::unordered_map<std::uint64_t, std::uint32_t> posId;
    posId.reserve(mesh.vertices.size());
    std::vector<std::uint32_t> vidToPid(mesh.vertices.size());
    for (std::uint32_t i = 0; i < mesh.vertices.size(); ++i) {
        const std::uint64_t k = pkey(mesh.vertices[i]);
        auto it = posId.find(k);
        if (it == posId.end()) {
            const std::uint32_t id = static_cast<std::uint32_t>(posId.size());
            posId.emplace(k, id);
            vidToPid[i] = id;
        } else {
            vidToPid[i] = it->second;
        }
    }

    // Group triangle edges by their undirected position-edge; store the original vertex at each endpoint.
    struct Occ { std::uint32_t vLo, vHi; }; // original vertex indices, vLo at the lower position id
    std::unordered_map<std::uint64_t, std::vector<Occ>> edges;
    edges.reserve(triN * 3);
    for (std::size_t t = 0; t < triN; ++t) {
        for (int e = 0; e < 3; ++e) {
            const std::uint32_t a = mesh.indices[t * 3 + static_cast<std::size_t>(e)];
            const std::uint32_t b = mesh.indices[t * 3 + static_cast<std::size_t>((e + 1) % 3)];
            const std::uint32_t pa = vidToPid[a], pb = vidToPid[b];
            if (pa == pb) continue; // degenerate edge (same position)
            std::uint32_t plo = pa, phi = pb, vlo = a, vhi = b;
            if (pb < pa) { plo = pb; phi = pa; vlo = b; vhi = a; }
            const std::uint64_t ek = (static_cast<std::uint64_t>(plo) << 32) | phi;
            edges[ek].push_back({vlo, vhi});
        }
    }

    auto uvDiffers = [&](std::uint32_t v0, std::uint32_t v1) {
        const MeshVertex& a = mesh.vertices[v0];
        const MeshVertex& b = mesh.vertices[v1];
        return std::fabs(a.u - b.u) > uvEps || std::fabs(a.v - b.v) > uvEps;
    };

    for (const auto& kv : edges) {
        const std::vector<Occ>& occ = kv.second;
        if (occ.size() == 1) { ++out.boundaryEdgeCount; continue; }
        if (occ.size() >= 3) { ++out.nonManifoldEdgeCount; continue; }
        ++out.interiorEdgeCount;
        // Interior: UV seam if the two faces disagree on either endpoint's UV.
        if (uvDiffers(occ[0].vLo, occ[1].vLo) || uvDiffers(occ[0].vHi, occ[1].vHi))
            out.seamEdges.push_back({occ[0].vLo, occ[0].vHi});
    }
    return out;
}

} // namespace maz::render
