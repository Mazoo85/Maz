#pragma once

#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology (M528)
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render TRIANGLE WINDING / NORMAL-CONSISTENCY DETECTOR — find the triangles whose winding (vertex order,
// which decides which way the face points) DISAGREES with their neighbours, so the mesh can be made uniformly
// outward-facing. Flipped faces are one of the most common import defects: they turn black under lighting, punch
// holes in shadows, and break backface culling and solid-mesh tests. In a consistently wound surface every shared
// edge is traversed in OPPOSITE directions by its two triangles; a flipped triangle traverses its shared edges
// the SAME way as its neighbours. This walks each connected surface from a seed, propagates a consistent
// orientation across shared edges (M528 half-edge topology), and reports the MINORITY set per component — the
// triangles you'd flip to make it uniform — plus the raw count of inconsistently-wound edges. It's the analysis
// behind a "recalculate / make normals consistent" command. Header-only, pure CPU.
//
// Scope note (honest): this reports which triangles disagree WITH EACH OTHER and the smaller set to flip per
// connected component; it does NOT decide which way is "out" (that needs a containment/volume test — pair with
// M546 containsPoint or a signed-volume check to orient outward). Non-orientable surfaces (a Möbius strip) have
// no consistent assignment — the walk still returns a best-effort labelling and the inconsistent-edge count
// stays non-zero. Boundary and non-manifold edges are simply not propagated across.
namespace maz::render {

struct WindingReport {
    std::vector<std::uint8_t> flipped;   // per-triangle: 1 = in the minority (disagreeing) set of its component
    std::size_t flippedCount = 0;        // total triangles to flip to make each component internally consistent
    std::size_t inconsistentEdges = 0;   // shared edges whose two triangles wind the SAME direction
    std::size_t componentsChecked = 0;   // connected triangle components visited
    bool consistent = false;             // true if the mesh is already uniformly wound (no inconsistent edges)
};

// Analyse the winding consistency of `mesh`.
inline WindingReport analyzeWinding(const shapes::MeshData& mesh) {
    WindingReport rep;
    const MeshTopology topo = buildTopology(mesh);
    const std::size_t triN = topo.triangleCount;
    rep.flipped.assign(triN, 0);
    if (triN == 0) {
        rep.consistent = true;
        return rep;
    }

    // Directed origin of a half-edge he = 3*t + e: indices[3t+e]. Two twins are consistently wound iff they run
    // the shared edge in opposite directions, i.e. their origins differ; same origin => one is flipped.
    auto heFrom = [&](std::uint32_t he) { return mesh.indices[he]; };

    // Count inconsistently-wound shared edges (each undirected edge counted once).
    for (std::uint32_t he = 0; he < topo.opposite.size(); ++he) {
        const std::uint32_t tw = topo.opposite[he];
        if (tw == MeshTopology::kNone || tw < he) continue;
        // Consistent winding => the twin runs the edge in reverse (twin.from == he.to). Same direction => flip.
        if (heFrom(tw) == heFrom(he)) ++rep.inconsistentEdges;
    }
    rep.consistent = (rep.inconsistentEdges == 0);

    // Propagate a consistent orientation across each connected component (2-colouring by "needs flip vs seed").
    std::vector<std::uint8_t> label(triN, 2); // 2 = unvisited
    std::vector<std::uint32_t> stack;
    stack.reserve(triN);
    for (std::uint32_t seed = 0; seed < triN; ++seed) {
        if (label[seed] != 2) continue;
        ++rep.componentsChecked;
        std::vector<std::uint32_t> members;
        label[seed] = 0;
        stack.push_back(seed);
        while (!stack.empty()) {
            const std::uint32_t t = stack.back();
            stack.pop_back();
            members.push_back(t);
            for (std::uint32_t e = 0; e < 3; ++e) {
                const std::uint32_t he = t * 3 + e;
                const std::uint32_t tw = topo.opposite[he];
                if (tw == MeshTopology::kNone) continue;
                const std::uint32_t t2 = tw / 3;
                if (label[t2] != 2) continue;
                // Same-direction shared edge => t2 disagrees with t => opposite label; reversed => same label.
                const std::uint8_t rel = (heFrom(tw) == heFrom(he)) ? std::uint8_t(1) : std::uint8_t(0);
                label[t2] = static_cast<std::uint8_t>(label[t] ^ rel);
                stack.push_back(t2);
            }
        }
        // Report the MINORITY as "flipped" (the smaller set to flip to make this component consistent).
        std::size_t ones = 0;
        for (std::uint32_t t : members) ones += label[t];
        const bool invert = ones * 2 > members.size(); // majority are 1 -> flip labelling so minority is 1
        for (std::uint32_t t : members) {
            const std::uint8_t f = invert ? static_cast<std::uint8_t>(1 - label[t]) : label[t];
            rep.flipped[t] = f;
            rep.flippedCount += f;
        }
    }
    return rep;
}

// Return a copy of `mesh` with the disagreeing (minority) triangles re-wound so the mesh is uniformly oriented
// within each connected component. (Does not decide outward vs inward — see the scope note.)
inline shapes::MeshData makeWindingConsistent(const shapes::MeshData& mesh) {
    shapes::MeshData out = mesh;
    const WindingReport rep = analyzeWinding(mesh);
    for (std::size_t t = 0; t < rep.flipped.size(); ++t) {
        if (rep.flipped[t]) {
            // Reverse the triangle: swap its 2nd and 3rd corners.
            const std::size_t b = t * 3 + 1, c = t * 3 + 2;
            const std::uint32_t tmp = out.indices[b];
            out.indices[b] = out.indices[c];
            out.indices[c] = tmp;
        }
    }
    return out;
}

} // namespace maz::render
