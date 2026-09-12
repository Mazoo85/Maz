#pragma once

#include "maz/math/Math.hpp"        // math::vec3, dot
#include "maz/render/MeshTools.hpp" // computeNormals
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render VERTEX-COLOR CAVITY (curvature) BAKE — write a mesh's own shape into its vertex colours so that
// CREVICES, GROOVES, and CONCAVE folds go DARK while RIDGES, EDGES, and CONVEX bulges go LIGHT, with no texture
// and no ray-tracing. This is the "cavity map" sculpting tools (ZBrush, Blender, Substance) overlay to make
// surface detail — panel-line grime, worn edges, carved seams — read at a glance; here it is baked straight into
// per-vertex RGB so any flat-lit or unlit renderer shows the form for free. It complements the M553 ambient-
// occlusion bake (which measures how BOXED-IN a point is by shooting rays) — cavity is purely LOCAL curvature
// (how the immediate neighbourhood folds), so it is far cheaper and sharpens fine creases AO misses.
//
// The signal is a signed concavity: for each vertex we take the vector from the vertex to the CENTROID of its
// edge-neighbours and project it onto the (smooth) surface normal, scaled by local edge length so it is
// resolution- and size-independent. Neighbours sitting FURTHER OUT along the normal than the vertex mean the
// vertex is RECESSED → concave → positive; neighbours pulled INWARD mean the vertex JUTS OUT → convex → negative;
// a flat neighbourhood gives ~0. Normals are recomputed internally (area-weighted), so the input needn't carry
// them. Header-only, pure CPU.
//
// Scope note (honest): this is a first-ring DISCRETE curvature estimate — a fast local proxy, not the exact
// mean-curvature of MeshCurvature (M539); it reads folds within one edge of a vertex, so detail finer than the
// tessellation is invisible. Values are unbounded in principle (a razor crease gives a large magnitude); the
// colour bake clamps them. Boundary vertices (open edges) see a lopsided neighbourhood and read less reliably.
namespace maz::render {

namespace detail {

// Per-vertex signed concavity used by the cavity bake. Positive = concave (pit/groove), negative = convex
// (ridge/edge), ~0 = flat. Scaled by local mean edge length so it is independent of mesh size and tessellation.
inline std::vector<float> cavitySignal(const shapes::MeshData& mesh) {
    const std::size_t vn = mesh.vertices.size();
    std::vector<float> out(vn, 0.0f);
    if (vn == 0 || mesh.indices.size() < 3) return out;

    std::vector<math::vec3> pos(vn);
    for (std::size_t i = 0; i < vn; ++i) {
        const MeshVertex& v = mesh.vertices[i];
        pos[i] = math::vec3(v.px, v.py, v.pz);
    }
    const std::vector<math::vec3> nrm = computeNormals(pos, mesh.indices);

    // Accumulate each vertex's neighbour-centroid and mean neighbour distance from the mesh edges. Every
    // undirected edge is counted once per incident triangle; degree cancels out when we divide by the count.
    std::vector<math::vec3> nbrSum(vn, math::vec3(0.0f, 0.0f, 0.0f));
    std::vector<double> distSum(vn, 0.0);
    std::vector<std::uint32_t> degree(vn, 0);
    const std::size_t triN = mesh.indices.size() / 3;
    auto addEdge = [&](std::uint32_t a, std::uint32_t b) {
        if (a >= vn || b >= vn) return;
        nbrSum[a] += pos[b];
        distSum[a] += std::sqrt(static_cast<double>(math::dot(pos[b] - pos[a], pos[b] - pos[a])));
        ++degree[a];
    };
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        addEdge(ia, ib);
        addEdge(ib, ia);
        addEdge(ib, ic);
        addEdge(ic, ib);
        addEdge(ic, ia);
        addEdge(ia, ic);
    }

    for (std::size_t i = 0; i < vn; ++i) {
        if (degree[i] == 0) continue;
        const float inv = 1.0f / static_cast<float>(degree[i]);
        const math::vec3 centroid = nbrSum[i] * inv;
        const float meanDist = static_cast<float>(distSum[i]) * inv;
        if (meanDist <= 1e-12f) continue;
        // Project the vertex->neighbour-centroid vector onto the outward normal, normalised by edge scale.
        out[i] = math::dot(centroid - pos[i], nrm[i]) / meanDist;
    }
    return out;
}

} // namespace detail

// Return the raw per-vertex signed cavity signal (positive = concave, negative = convex, ~0 = flat). Exposed so
// callers can drive their own shading ramp or thresholds; the colour bake below is the batteries-included path.
inline std::vector<float> computeCavity(const shapes::MeshData& mesh) { return detail::cavitySignal(mesh); }

// Return a copy of `mesh` with the cavity signal modulated into each vertex's RGB: concave crevices darken and
// convex ridges lighten. `strength` (0..1) scales the effect; `contrast` (>0) amplifies the raw signal before
// clamping to [-1,1] (higher = crisper creases). Channels are clamped to [0,1], so the result stays displayable.
inline shapes::MeshData bakeCavityToVertexColor(const shapes::MeshData& mesh, float strength = 1.0f,
                                                float contrast = 4.0f) {
    shapes::MeshData out = mesh;
    const std::vector<float> cav = detail::cavitySignal(mesh);
    const float s = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    for (std::size_t i = 0; i < out.vertices.size() && i < cav.size(); ++i) {
        float k = cav[i] * contrast;
        k = k < -1.0f ? -1.0f : (k > 1.0f ? 1.0f : k);
        const float factor = 1.0f - k * s; // concave (k>0) -> <1 dark; convex (k<0) -> >1 light
        MeshVertex& v = out.vertices[i];
        v.r = std::clamp(v.r * factor, 0.0f, 1.0f);
        v.g = std::clamp(v.g * factor, 0.0f, 1.0f);
        v.b = std::clamp(v.b * factor, 0.0f, 1.0f);
    }
    return out;
}

} // namespace maz::render
