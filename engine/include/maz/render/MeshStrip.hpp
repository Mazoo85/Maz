#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData (convenience overload)

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render TRIANGLE-STRIP GENERATION — repack an indexed triangle LIST into triangle STRIPS. A strip stores a
// run of triangles as one vertex sequence v0 v1 v2 v3 ..., where every new vertex forms a triangle with the
// previous two (GPU GL_TRIANGLE_STRIP / VK primitive-restart semantics), so N connected triangles cost N+2
// indices instead of 3N. Strips shrink index bandwidth, feed fixed-function / mobile / retro GPU paths that
// prefer strips, and are the on-disk form some formats (MD2/MD3, PS2/GameCube era) want. This is a simple
// greedy stripifier: start a triangle, orient it so its trailing edge has an un-stripped neighbour, then walk
// neighbour-to-neighbour across the trailing edge until the run dead-ends; separate runs by a restart index.
// `expandTriangleStrips` is the exact inverse (strips -> triangle list), so the pair round-trips the triangle
// SET losslessly. Header-only, std-only, deterministic.
//
// Scope note (honest): greedy, not length-optimal (NvTriStrip / tipsify find longer runs); it minimises index
// count opportunistically, not maximally. Winding within a strip alternates per GPU convention, so the expanded
// list preserves the triangle SET (same three vertices per face) though a face's winding may be normalised.
namespace maz::render {

struct TriangleStrips {
    std::vector<std::uint32_t> indices; // strip vertices, runs separated by `restartIndex`
    std::uint32_t restartIndex = 0xFFFFFFFFu;
    std::size_t stripCount = 0;
    std::size_t triangleCount = 0;      // triangles represented (== input triangle count)

    // Index count vs an unstripped list (3*triangleCount). runs cost tri+2 each -> tri + 2*stripCount verts.
    std::size_t indexCountVerts() const { return triangleCount + 2 * stripCount; }
};

// Build triangle strips from a flat triangle-list index buffer (multiple of 3). Degenerate triangles (a
// repeated corner) are skipped. `restart` is the sentinel separating runs; keep it distinct from real indices.
inline TriangleStrips buildTriangleStrips(const std::vector<std::uint32_t>& triList,
                                          std::uint32_t restart = 0xFFFFFFFFu) {
    TriangleStrips out;
    out.restartIndex = restart;
    const std::size_t triN = triList.size() / 3;
    if (triN == 0) return out;

    struct Tri { std::uint32_t v[3]; };
    std::vector<Tri> tris;
    tris.reserve(triN);
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t a = triList[t * 3 + 0], b = triList[t * 3 + 1], c = triList[t * 3 + 2];
        if (a == b || b == c || a == c) continue; // drop degenerate
        tris.push_back({{a, b, c}});
    }
    const std::uint32_t nT = static_cast<std::uint32_t>(tris.size());
    out.triangleCount = nT;
    if (nT == 0) return out;

    auto edgeKey = [](std::uint32_t x, std::uint32_t y) {
        return x < y ? (static_cast<std::uint64_t>(x) << 32) | y : (static_cast<std::uint64_t>(y) << 32) | x;
    };
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> edgeTris;
    edgeTris.reserve(nT * 3);
    for (std::uint32_t t = 0; t < nT; ++t) {
        edgeTris[edgeKey(tris[t].v[0], tris[t].v[1])].push_back(t);
        edgeTris[edgeKey(tris[t].v[1], tris[t].v[2])].push_back(t);
        edgeTris[edgeKey(tris[t].v[2], tris[t].v[0])].push_back(t);
    }

    std::vector<std::uint8_t> used(nT, 0);
    auto thirdVertex = [](const Tri& tri, std::uint32_t a, std::uint32_t b) {
        for (int k = 0; k < 3; ++k)
            if (tri.v[k] != a && tri.v[k] != b) return tri.v[k];
        return tri.v[0];
    };
    // First unused triangle (other than `self`) sharing undirected edge (a,b); 0xFFFFFFFF if none.
    auto neighbourAcross = [&](std::uint32_t a, std::uint32_t b, std::uint32_t self) -> std::uint32_t {
        auto it = edgeTris.find(edgeKey(a, b));
        if (it == edgeTris.end()) return 0xFFFFFFFFu;
        for (std::uint32_t cand : it->second)
            if (cand != self && !used[cand]) return cand;
        return 0xFFFFFFFFu;
    };

    for (std::uint32_t s = 0; s < nT; ++s) {
        if (used[s]) continue;
        // Orient the start triangle so its trailing edge (last two verts) has an unused neighbour, if possible.
        const Tri& t0 = tris[s];
        std::uint32_t order[3] = {t0.v[0], t0.v[1], t0.v[2]};
        const std::uint32_t rots[3][3] = {{t0.v[0], t0.v[1], t0.v[2]},
                                          {t0.v[1], t0.v[2], t0.v[0]},
                                          {t0.v[2], t0.v[0], t0.v[1]}};
        for (const auto& r : rots) {
            if (neighbourAcross(r[1], r[2], s) != 0xFFFFFFFFu) { order[0] = r[0]; order[1] = r[1]; order[2] = r[2]; break; }
        }
        used[s] = 1;
        std::vector<std::uint32_t> strip = {order[0], order[1], order[2]};
        std::uint32_t cur = s;
        while (true) {
            const std::uint32_t e0 = strip[strip.size() - 2], e1 = strip[strip.size() - 1];
            const std::uint32_t nb = neighbourAcross(e0, e1, cur);
            if (nb == 0xFFFFFFFFu) break;
            used[nb] = 1;
            strip.push_back(thirdVertex(tris[nb], e0, e1));
            cur = nb;
        }
        if (!out.indices.empty()) out.indices.push_back(restart);
        for (std::uint32_t v : strip) out.indices.push_back(v);
        ++out.stripCount;
    }
    return out;
}

// Convenience: strip a mesh's index buffer directly.
inline TriangleStrips buildTriangleStrips(const shapes::MeshData& mesh, std::uint32_t restart = 0xFFFFFFFFu) {
    return buildTriangleStrips(mesh.indices, restart);
}

// Expand triangle strips back into a flat triangle-list index buffer (the exact inverse of the packing).
inline std::vector<std::uint32_t> expandTriangleStrips(const TriangleStrips& strips) {
    std::vector<std::uint32_t> out;
    std::vector<std::uint32_t> run;
    auto flush = [&]() {
        for (std::size_t i = 0; i + 2 < run.size(); ++i) {
            std::uint32_t x = run[i], y = run[i + 1], z = run[i + 2];
            if (i & 1) { const std::uint32_t tmp = x; x = y; y = tmp; } // alternate winding per GPU convention
            out.push_back(x); out.push_back(y); out.push_back(z);
        }
        run.clear();
    };
    for (std::uint32_t v : strips.indices) {
        if (v == strips.restartIndex) { flush(); continue; }
        run.push_back(v);
    }
    flush();
    return out;
}

} // namespace maz::render
