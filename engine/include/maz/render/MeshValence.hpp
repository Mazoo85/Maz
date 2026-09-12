#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// maz::render VERTEX VALENCE / IRREGULAR-VERTEX REPORT — count how many edges meet at each vertex (its VALENCE)
// and flag the IRREGULAR ones. In a clean triangle mesh almost every interior vertex has valence 6 (six triangles
// fanning around it); vertices that don't — the 5s and 7s, called poles or singularities — are where edge flow
// pinches or splays. Retopology and subdivision tools work hard to MINIMISE them because irregular vertices cause
// shading artefacts, uneven subdivision, and awkward UV/animation deformation. This report gives the per-vertex
// valence, marks boundary vertices (open edges, which are naturally lower-valence and judged separately), and
// tallies how many interior vertices are regular (6) vs irregular — a one-number read on mesh quality that a
// modelling tool shows as a "show poles" overlay or a retopo score. Header-only, pure CPU; no topology build
// needed — it counts distinct edge-neighbours and single-face (boundary) edges directly.
//
// Scope note (honest): "regular = 6" is the triangle-mesh convention; a quad mesh's ideal is valence 4, so read
// `irregularInterior` accordingly for quad-derived data. Valence counts DISTINCT connected neighbours, so a
// non-manifold or duplicated-vertex mesh can report surprising values — weld first (see M525/M559). Boundary
// vertices are reported and counted but never labelled irregular, since their low valence is expected.
namespace maz::render {

struct ValenceReport {
    std::vector<std::uint32_t> valence; // distinct edge-neighbours per vertex
    std::vector<std::uint8_t> boundary; // 1 if the vertex touches an open (single-face) edge
    std::uint32_t minValence = 0;
    std::uint32_t maxValence = 0;
    float meanValence = 0.0f;
    std::size_t regularInterior = 0;   // interior vertices with valence exactly 6
    std::size_t irregularInterior = 0; // interior vertices with valence != 6 (poles / singularities)
    std::size_t boundaryVertices = 0;
    std::size_t isolatedVertices = 0;  // valence 0 (unreferenced by any triangle)
};

// Compute the valence report for `mesh`. `regularValence` is the "ideal" interior valence (6 for triangles).
inline ValenceReport analyzeValence(const shapes::MeshData& mesh, std::uint32_t regularValence = 6) {
    ValenceReport rep;
    const std::size_t n = mesh.vertices.size();
    rep.valence.assign(n, 0);
    rep.boundary.assign(n, 0);
    if (n == 0) return rep;

    // Distinct neighbours per vertex, and how many triangles use each undirected edge.
    std::vector<std::unordered_set<std::uint32_t>> nbr(n);
    std::unordered_map<std::uint64_t, std::uint32_t> edgeFaces; // packed (min,max) -> incident-triangle count
    auto key = [](std::uint32_t a, std::uint32_t b) {
        if (a > b) { const std::uint32_t t = a; a = b; b = t; }
        return (static_cast<std::uint64_t>(a) << 32) | static_cast<std::uint64_t>(b);
    };

    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t v[3] = {mesh.indices[t * 3 + 0], mesh.indices[t * 3 + 1], mesh.indices[t * 3 + 2]};
        for (int e = 0; e < 3; ++e) {
            const std::uint32_t a = v[e], b = v[(e + 1) % 3];
            if (a >= n || b >= n || a == b) continue;
            nbr[a].insert(b);
            nbr[b].insert(a);
            ++edgeFaces[key(a, b)];
        }
    }
    // Any edge used by exactly one triangle is a boundary edge; its endpoints are boundary vertices.
    for (const auto& kv : edgeFaces) {
        if (kv.second == 1) {
            const std::uint32_t a = static_cast<std::uint32_t>(kv.first >> 32);
            const std::uint32_t b = static_cast<std::uint32_t>(kv.first & 0xFFFFFFFFu);
            rep.boundary[a] = 1;
            rep.boundary[b] = 1;
        }
    }

    std::uint32_t mn = 0xFFFFFFFFu, mx = 0;
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t val = static_cast<std::uint32_t>(nbr[i].size());
        rep.valence[i] = val;
        sum += val;
        if (val < mn) mn = val;
        if (val > mx) mx = val;
        if (val == 0) {
            ++rep.isolatedVertices;
        } else if (rep.boundary[i]) {
            ++rep.boundaryVertices;
        } else if (val == regularValence) {
            ++rep.regularInterior;
        } else {
            ++rep.irregularInterior;
        }
    }
    rep.minValence = (n > 0) ? mn : 0;
    rep.maxValence = mx;
    rep.meanValence = static_cast<float>(static_cast<double>(sum) / static_cast<double>(n));
    return rep;
}

} // namespace maz::render
