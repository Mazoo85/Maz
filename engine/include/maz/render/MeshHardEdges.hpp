#pragma once

#include "maz/math/Math.hpp"           // math::vec3, cross, dot
#include "maz/render/MeshTopology.hpp" // buildTopology, MeshTopology
#include "maz/render/Shapes.hpp"       // shapes::MeshData, MeshVertex

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// maz::render HARD-EDGE / SMOOTHING-GROUP split by crease angle — the importer step that decides where a
// surface should shade SMOOTH (normals averaged across an edge) versus FLAT (a crisp crease): every edge
// whose two faces meet at more than the crease angle is a hard edge, and the shared vertices along it are
// DUPLICATED so the smooth-normal averaging doesn't bleed across the fold. This is Godot's import "Normals >
// From Smoothing Groups" / the shade-smooth-by-angle operation, and the correct front end to computeNormals:
// a raw cube welded to 8 vertices would otherwise get rounded, mushy corners. Built on the M528 topology:
// around each vertex the incident triangles are grouped (union-find) so neighbours joined by a SUB-threshold
// edge stay together, and each group becomes one output vertex with its own averaged normal. Pure CPU,
// header-only, headless.
namespace maz::render {

// Return a copy of `mesh` with vertices split along edges sharper than `creaseAngleDegrees`, and per-vertex
// normals recomputed within each smoothing group (area-weighted). The triangle set is preserved; only the
// vertex list grows (shared verts on a crease are duplicated). A large angle (>= 180) keeps everything
// welded/smooth; a small angle splits every crease (flat shading).
inline shapes::MeshData splitHardEdges(const shapes::MeshData& mesh, float creaseAngleDegrees) {
    shapes::MeshData out;
    if (mesh.indices.size() < 3) { out = mesh; return out; }

    const MeshTopology topo = buildTopology(mesh);
    const std::uint32_t triCount = topo.triangleCount;
    const float cosCrease = std::cos(creaseAngleDegrees * 3.14159265358979323846f / 180.0f);

    // Unnormalized (area-weighted) face normals.
    std::vector<math::vec3> faceN(triCount);
    auto vpos = [&](std::uint32_t idx) {
        const MeshVertex& v = mesh.vertices[idx];
        return math::vec3(v.px, v.py, v.pz);
    };
    for (std::uint32_t t = 0; t < triCount; ++t) {
        const math::vec3 a = vpos(mesh.indices[t * 3]), b = vpos(mesh.indices[t * 3 + 1]), c = vpos(mesh.indices[t * 3 + 2]);
        faceN[t] = math::cross(b - a, c - a);
    }
    auto smoothAcross = [&](std::uint32_t t0, std::uint32_t t1) {
        const math::vec3 &n0 = faceN[t0], &n1 = faceN[t1];
        const float l0 = std::sqrt(math::dot(n0, n0)), l1 = std::sqrt(math::dot(n1, n1));
        if (l0 < 1e-20f || l1 < 1e-20f) return true; // treat degenerate as smooth (don't force a split)
        return (math::dot(n0, n1) / (l0 * l1)) >= cosCrease; // angle <= crease => smooth => keep together
    };

    // Incident triangles per original vertex.
    std::vector<std::vector<std::uint32_t>> inc(mesh.vertices.size());
    for (std::uint32_t t = 0; t < triCount; ++t)
        for (int c = 0; c < 3; ++c) inc[mesh.indices[t * 3 + static_cast<std::uint32_t>(c)]].push_back(t);

    // For each triangle corner, the output vertex index it will reference.
    std::vector<std::array<std::uint32_t, 3>> corner(triCount);

    for (std::uint32_t v = 0; v < mesh.vertices.size(); ++v) {
        const std::vector<std::uint32_t>& tris = inc[v];
        if (tris.empty()) continue;
        // Local union-find over this vertex's incident triangles.
        std::unordered_map<std::uint32_t, std::uint32_t> local; // triangleId -> local slot
        for (std::uint32_t i = 0; i < tris.size(); ++i) local[tris[i]] = i;
        std::vector<std::uint32_t> parent(tris.size());
        for (std::uint32_t i = 0; i < parent.size(); ++i) parent[i] = i;
        std::function<std::uint32_t(std::uint32_t)> find = [&](std::uint32_t x) {
            while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
            return x;
        };
        // Union triangles that share a v-incident edge and shade smooth across it.
        for (std::uint32_t t : tris) {
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t va = mesh.indices[t * 3 + static_cast<std::uint32_t>(e)];
                const std::uint32_t vb = mesh.indices[t * 3 + static_cast<std::uint32_t>((e + 1) % 3)];
                if (va != v && vb != v) continue; // only edges that touch v
                const std::uint32_t nb = topo.triangleNeighbor(t, e);
                if (nb == MeshTopology::kNone) continue;
                auto it = local.find(nb);
                if (it == local.end()) continue; // not around v (non-manifold guard)
                if (smoothAcross(t, nb)) parent[find(local[t])] = find(it->second);
            }
        }
        // Each root -> a fresh output vertex; record the mapping for every incident triangle corner.
        std::unordered_map<std::uint32_t, std::uint32_t> rootToOut;
        for (std::uint32_t t : tris) {
            const std::uint32_t root = find(local[t]);
            std::uint32_t outIdx;
            auto it = rootToOut.find(root);
            if (it == rootToOut.end()) {
                outIdx = static_cast<std::uint32_t>(out.vertices.size());
                rootToOut[root] = outIdx;
                out.vertices.push_back(mesh.vertices[v]); // carry attributes; normal recomputed below
            } else {
                outIdx = it->second;
            }
            for (int c = 0; c < 3; ++c)
                if (mesh.indices[t * 3 + static_cast<std::uint32_t>(c)] == v) corner[t][static_cast<std::size_t>(c)] = outIdx;
        }
    }

    // Emit indices and accumulate group normals.
    out.indices.reserve(mesh.indices.size());
    std::vector<math::vec3> accN(out.vertices.size(), math::vec3(0, 0, 0));
    for (std::uint32_t t = 0; t < triCount; ++t) {
        for (int c = 0; c < 3; ++c) {
            const std::uint32_t oi = corner[t][static_cast<std::size_t>(c)];
            out.indices.push_back(oi);
            accN[oi] += faceN[t];
        }
    }
    for (std::size_t i = 0; i < out.vertices.size(); ++i) {
        const float l = std::sqrt(math::dot(accN[i], accN[i]));
        const math::vec3 n = l > 1e-20f ? accN[i] / l : math::vec3(0, 1, 0);
        out.vertices[i].nx = n.x; out.vertices[i].ny = n.y; out.vertices[i].nz = n.z;
    }
    return out;
}

} // namespace maz::render
