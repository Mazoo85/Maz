#pragma once

#include "maz/render/MeshSmooth.hpp" // detail::buildAdjacency (edge-neighbour sets)
#include "maz/render/Shapes.hpp"     // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render VERTEX-COLOUR SMOOTHING — blur a mesh's per-vertex RGB across its edges without moving a single
// vertex. Baked vertex colours — ambient occlusion (M553), cavity/curvature (M556), hand-painted masks — often
// come out noisy or blocky: a low ray count leaves AO speckled, a coarse mesh makes cavity shading stair-step,
// and a paint stroke lands hard-edged. This relaxes each vertex's colour toward the average of its edge-neighbours
// (a Laplacian blur on the colour signal, exactly like M540 mesh smoothing but on COLOUR, not POSITION), so the
// shading reads soft and clean while the geometry stays bit-for-bit identical. `strength` (0..1) sets the blur per
// pass and `iterations` how many passes; boundary vertices can be pinned so open edges keep their colour. Reuses
// the M540 adjacency builder. Header-only, pure CPU.
//
// Scope note (honest): this smooths ONLY the RGB channels; positions, normals, and UVs are untouched. It is an
// unweighted (umbrella) Laplacian — neighbour count, not edge length or angle, sets the weight — which is fast
// and stable but slightly blurs across sharp colour boundaries; drop `strength`/`iterations` or pin boundaries to
// preserve edges. Colours are read and written in the [0,1] range the vertices already store; nothing is clamped
// beyond staying within the neighbours' own range, so a valid input stays valid.
namespace maz::render {

// Return a copy of `mesh` with per-vertex RGB Laplacian-smoothed. `strength` (0..1) is the blend toward the
// neighbour-average each pass; `iterations` repeats it; `pinBoundary` keeps open-edge vertices unchanged.
inline shapes::MeshData smoothVertexColors(const shapes::MeshData& mesh, float strength = 0.5f,
                                           int iterations = 1, bool pinBoundary = false) {
    shapes::MeshData out = mesh;
    const std::size_t n = out.vertices.size();
    if (n == 0 || mesh.indices.size() < 3 || iterations < 1) return out;

    std::vector<std::vector<std::uint32_t>> nbr;
    std::vector<bool> boundary;
    detail::buildAdjacency(n, mesh.indices, nbr, boundary);

    const float s = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    // Work on parallel RGB arrays so every vertex is updated from the SAME source snapshot each pass.
    std::vector<float> r(n), g(n), b(n);
    for (std::size_t i = 0; i < n; ++i) {
        r[i] = out.vertices[i].r;
        g[i] = out.vertices[i].g;
        b[i] = out.vertices[i].b;
    }
    std::vector<float> nr(n), ng(n), nb(n);
    for (int it = 0; it < iterations; ++it) {
        for (std::size_t i = 0; i < n; ++i) {
            if (nbr[i].empty() || (pinBoundary && boundary[i])) {
                nr[i] = r[i];
                ng[i] = g[i];
                nb[i] = b[i];
                continue;
            }
            float ar = 0.0f, ag = 0.0f, ab = 0.0f;
            for (std::uint32_t j : nbr[i]) {
                ar += r[j];
                ag += g[j];
                ab += b[j];
            }
            const float inv = 1.0f / static_cast<float>(nbr[i].size());
            ar *= inv;
            ag *= inv;
            ab *= inv;
            nr[i] = r[i] + (ar - r[i]) * s; // move a fraction `s` toward the neighbour average
            ng[i] = g[i] + (ag - g[i]) * s;
            nb[i] = b[i] + (ab - b[i]) * s;
        }
        r.swap(nr);
        g.swap(ng);
        b.swap(nb);
    }
    for (std::size_t i = 0; i < n; ++i) {
        out.vertices[i].r = std::clamp(r[i], 0.0f, 1.0f);
        out.vertices[i].g = std::clamp(g[i], 0.0f, 1.0f);
        out.vertices[i].b = std::clamp(b[i], 0.0f, 1.0f);
    }
    return out;
}

} // namespace maz::render
