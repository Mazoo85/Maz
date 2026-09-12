#pragma once

#include "maz/math/Math.hpp"     // math::vec3, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render MESH PLANE SLICE / CROSS-SECTION — intersect a triangle mesh with an infinite plane and return
// the CONTOUR: the line segments where the surface crosses the plane, chained into ordered (closed on a
// watertight solid) polyline loops. This is the cross-section a CAD tool draws, and the building block for
// cutaway / section views, a waterline or lava-line on a hull, terrain contour ("topographic") lines at a set
// of heights, silhouette/outline extraction, deriving a 2D collision outline from a 3D prop, and 3D-printing
// slicers. Each triangle straddling the plane contributes one segment between the crossing points on its two
// sign-changing edges. The crossing on each mesh edge is keyed by that undirected edge (the two triangles
// sharing it produce the SAME point), so on a manifold every contour point has exactly two incident segments
// and the chain closes into clean loops. Reuses only vec3/dot from maz::math. Header-only, deterministic,
// headless — a cube sliced through its middle yields a square loop.
//
// Scope note (honest): the plane is {x : dot(normal, x) == offset}; `normal` sets orientation and need not be
// unit length. A vertex exactly on the plane counts to the non-positive side, and a triangle lying flat in the
// plane contributes no 1D contour (both are documented). Chaining assumes a manifold cut; a non-manifold edge
// (>2 triangles) may leave an open chain, which is reported (loopClosed==0), never silently closed.
namespace maz::render {

struct SliceContour {
    std::vector<math::vec3> points;                // unique crossing points (one per crossed mesh edge)
    std::vector<std::vector<std::uint32_t>> loops; // ordered indices into `points`
    std::vector<std::uint8_t> loopClosed;          // 1 if the loop returns to its start (a proper ring)
    std::size_t segmentCount = 0;                  // raw straddling-triangle segments before chaining

    std::size_t closedLoopCount() const {
        std::size_t n = 0;
        for (std::uint8_t c : loopClosed) n += c;
        return n;
    }
};

// Slice `mesh` by the plane {x : dot(normal, x) == offset}. Empty/degenerate mesh yields an empty contour.
inline SliceContour sliceMesh(const shapes::MeshData& mesh, const math::vec3& normal, float offset) {
    SliceContour out;
    if (mesh.indices.size() < 3 || mesh.vertices.empty()) return out;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };
    auto signedDist = [&](const math::vec3& p) { return math::dot(normal, p) - offset; };

    // One crossing point per crossed undirected mesh edge; shared edges resolve to the SAME point index.
    std::unordered_map<std::uint64_t, std::uint32_t> edgeMap;
    edgeMap.reserve(mesh.indices.size());
    auto edgeCross = [&](std::uint32_t i0, const math::vec3& p0, float d0,
                         std::uint32_t i1, const math::vec3& p1, float d1, std::uint32_t& outIdx) -> bool {
        if ((d0 > 0.0f) == (d1 > 0.0f)) return false; // same side, no crossing
        const std::uint64_t ek = i0 < i1 ? (static_cast<std::uint64_t>(i0) << 32) | i1
                                         : (static_cast<std::uint64_t>(i1) << 32) | i0;
        auto it = edgeMap.find(ek);
        if (it != edgeMap.end()) { outIdx = it->second; return true; }
        const float tpar = d0 / (d0 - d1); // linear interpolation to the zero-crossing
        const math::vec3 x = p0 + (p1 - p0) * tpar;
        outIdx = static_cast<std::uint32_t>(out.points.size());
        out.points.push_back(x);
        edgeMap.emplace(ek, outIdx);
        return true;
    };

    std::vector<std::pair<std::uint32_t, std::uint32_t>> segs;
    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) continue;
        const math::vec3 pa = pos(ia), pb = pos(ib), pc = pos(ic);
        const float da = signedDist(pa), db = signedDist(pb), dc = signedDist(pc);

        std::uint32_t hits[3];
        int nHits = 0;
        std::uint32_t h;
        if (edgeCross(ia, pa, da, ib, pb, db, h) && nHits < 3) hits[nHits++] = h;
        if (edgeCross(ib, pb, db, ic, pc, dc, h) && nHits < 3) hits[nHits++] = h;
        if (edgeCross(ic, pc, dc, ia, pa, da, h) && nHits < 3) hits[nHits++] = h;
        if (nHits == 2 && hits[0] != hits[1]) segs.push_back({hits[0], hits[1]});
    }
    out.segmentCount = segs.size();
    if (segs.empty()) return out;

    // Chain segments into loops via point -> incident-segment adjacency (degree 2 on a manifold cut).
    std::vector<std::vector<std::uint32_t>> adj(out.points.size());
    for (std::uint32_t s = 0; s < segs.size(); ++s) {
        adj[segs[s].first].push_back(s);
        adj[segs[s].second].push_back(s);
    }
    std::vector<std::uint8_t> usedSeg(segs.size(), 0);
    auto other = [&](std::uint32_t s, std::uint32_t p) {
        return segs[s].first == p ? segs[s].second : segs[s].first;
    };
    for (std::uint32_t s0 = 0; s0 < segs.size(); ++s0) {
        if (usedSeg[s0]) continue;
        std::vector<std::uint32_t> loop;
        const std::uint32_t start = segs[s0].first;
        std::uint32_t cur = start;
        std::uint32_t seg = s0;
        loop.push_back(cur);
        while (true) {
            usedSeg[seg] = 1;
            const std::uint32_t nxt = other(seg, cur);
            if (nxt == start) { out.loopClosed.push_back(1); break; } // closed the ring
            loop.push_back(nxt);
            std::uint32_t following = 0xFFFFFFFFu;
            for (std::uint32_t cand : adj[nxt])
                if (!usedSeg[cand]) { following = cand; break; }
            if (following == 0xFFFFFFFFu) { out.loopClosed.push_back(0); break; } // open chain (non-manifold)
            cur = nxt;
            seg = following;
        }
        out.loops.push_back(std::move(loop));
    }
    return out;
}

} // namespace maz::render
