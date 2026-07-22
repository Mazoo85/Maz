#pragma once

#include "maz/math/Math.hpp"       // math::vec3
#include "maz/render/MeshTools.hpp" // computeNormals (area-weighted smooth normals)
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render SOLIDIFY / SHELL — give a paper-thin surface real THICKNESS: take a one-sided sheet (a plane, a
// curved patch, a cloth, an open terrain skirt, a leaf) and turn it into a closed solid slab with a front face, a
// back face, and a rim sealing the two together along the open edges. This is Blender's "Solidify" modifier and
// the standard fix for surfaces that look like they vanish when seen edge-on or that leak light because they have
// no back: a wall built from a single quad, an imported single-sided mesh, a heightmap patch you want to render
// as a solid block. The back face is the front pushed inward along each vertex's (smooth) normal by `thickness`
// and wound the opposite way; the rim bridges every boundary (open) edge, so the result is watertight whenever
// the input was a clean manifold-with-boundary. Reuses the engine's area-weighted `computeNormals`. Header-only.
//
// Scope note (honest): the shell offsets each vertex straight along its smooth normal — correct for gentle
// surfaces, but on a very sharp concave crease the inner offsets can cross and self-intersect (Blender's
// "complex" solidify avoids this; this is the fast "simple" mode). The rim reuses the existing front/back
// vertices rather than adding creased rim vertices, so rim shading is smooth rather than hard-edged (run
// `computeNormals`/facet after if you want crisp rim edges). `thickness` may be negative to push the shell the
// other way. A CLOSED input (no boundary edges) just gets a second inner shell and no rim (`hadBoundary=false`).
namespace maz::render {

struct SolidifyResult {
    shapes::MeshData mesh;              // front + back + rim, one solid
    std::uint32_t rimTriangles = 0;     // 2 per boundary edge of the original surface
    bool hadBoundary = false;           // was the input an open surface (did it need a rim)?
};

// Thicken `mesh` into a solid slab of the given `thickness` (offset inward along smooth vertex normals).
inline SolidifyResult solidifyMesh(const shapes::MeshData& mesh, float thickness) {
    SolidifyResult out;
    const std::uint32_t n = static_cast<std::uint32_t>(mesh.vertices.size());
    if (n == 0 || mesh.indices.size() < 3) {
        out.mesh = mesh;
        return out;
    }

    // Smooth per-vertex normals from the geometry (independent of whatever the stored normals are).
    std::vector<math::vec3> pos(n);
    for (std::uint32_t i = 0; i < n; ++i)
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
    const std::vector<math::vec3> nrm = computeNormals(pos, mesh.indices);

    shapes::MeshData& r = out.mesh;
    r.vertices.reserve(static_cast<std::size_t>(n) * 2);

    // Front layer: the original vertices, unchanged.
    for (std::uint32_t i = 0; i < n; ++i) r.vertices.push_back(mesh.vertices[i]);

    // Back layer: pushed inward along -normal by `thickness`, with the normal flipped to face the other way.
    for (std::uint32_t i = 0; i < n; ++i) {
        MeshVertex v = mesh.vertices[i];
        v.px -= nrm[i].x * thickness;
        v.py -= nrm[i].y * thickness;
        v.pz -= nrm[i].z * thickness;
        v.nx = -v.nx;
        v.ny = -v.ny;
        v.nz = -v.nz;
        r.vertices.push_back(v);
    }

    const std::size_t triCount = mesh.indices.size() / 3;

    // Front triangles keep their winding; back triangles are the reverse, remapped onto the back layer.
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t a = mesh.indices[t * 3 + 0];
        const std::uint32_t b = mesh.indices[t * 3 + 1];
        const std::uint32_t c = mesh.indices[t * 3 + 2];
        r.indices.push_back(a);
        r.indices.push_back(b);
        r.indices.push_back(c);
        r.indices.push_back(a + n); // reversed order -> opposite facing
        r.indices.push_back(c + n);
        r.indices.push_back(b + n);
    }

    // Find boundary edges: an undirected edge used by exactly one triangle. Keep the DIRECTED edge from that
    // triangle so the rim quad is wound consistently with the front face.
    struct EdgeInfo { std::uint32_t a, b, count; };
    std::unordered_map<std::uint64_t, EdgeInfo> edges;
    edges.reserve(mesh.indices.size());
    auto key = [](std::uint32_t x, std::uint32_t y) {
        const std::uint32_t lo = x < y ? x : y, hi = x < y ? y : x;
        return (static_cast<std::uint64_t>(lo) << 32) | hi;
    };
    auto addEdge = [&](std::uint32_t x, std::uint32_t y) {
        const std::uint64_t k = key(x, y);
        auto it = edges.find(k);
        if (it == edges.end()) edges.emplace(k, EdgeInfo{x, y, 1});
        else ++it->second.count;
    };
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t a = mesh.indices[t * 3 + 0];
        const std::uint32_t b = mesh.indices[t * 3 + 1];
        const std::uint32_t c = mesh.indices[t * 3 + 2];
        addEdge(a, b);
        addEdge(b, c);
        addEdge(c, a);
    }

    // Rim: bridge each boundary edge (a->b) to its back copies (a+n, b+n) with two triangles.
    for (const auto& kv : edges) {
        if (kv.second.count != 1) continue;
        const std::uint32_t a = kv.second.a, b = kv.second.b;
        out.hadBoundary = true;
        r.indices.push_back(a);
        r.indices.push_back(b);
        r.indices.push_back(b + n);
        r.indices.push_back(a);
        r.indices.push_back(b + n);
        r.indices.push_back(a + n);
        out.rimTriangles += 2;
    }

    return out;
}

} // namespace maz::render
