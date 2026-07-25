#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot, normalize
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render PER-VERTEX AMBIENT OCCLUSION bake — the offline "bake AO into the mesh" step that darkens
// crevices, contact points, and interiors so a scene reads with depth even under flat ambient light, exactly
// what Godot's LightmapGI / the classic "vertex bake" does but stored per vertex. For each vertex it shoots a
// deterministic fan of rays over the hemisphere around the vertex normal and measures how many are blocked by
// the mesh's own triangles within a distance: fully open -> 0, deep in a cavity -> approaching 1. Ray/triangle
// tests use Möller–Trumbore. Fully deterministic (a golden-angle hemisphere set, no RNG) so it unit-tests by
// asserting an occluded vertex is darker than an exposed one. Pure CPU, header-only, headless.
//
// Scope note (honest): brute-force O(verts · rays · tris) against the mesh's own geometry — fine for the
// offline bake of props and levels. The BVH acceleration this note used to list as a follow-up now exists as
// render::MeshRayBvh (M547) — build one over the mesh and call occluded() per ray to cut the tris tested per
// ray from all-of-them to ~O(log tris); multi-bounce colour bleed remains the other documented follow-up.
// Normals are computed area-weighted from the triangles, so the input needs no pre-baked normals.
namespace maz::render {

namespace detail {

// Möller–Trumbore ray/triangle intersection; returns true and sets `t` (hit distance) on a forward hit.
inline bool aoRayTri(const math::vec3& o, const math::vec3& d, const math::vec3& a, const math::vec3& b,
                     const math::vec3& c, float maxDist, float& t) {
    const math::vec3 e1 = b - a, e2 = c - a;
    const math::vec3 p = math::cross(d, e2);
    const float det = math::dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const math::vec3 tv = o - a;
    const float u = math::dot(tv, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const math::vec3 q = math::cross(tv, e1);
    const float v = math::dot(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = math::dot(e2, q) * inv;
    return t > 1e-4f && t < maxDist;
}

} // namespace detail

// Bake per-vertex ambient occlusion into [0,1] (0 = fully open, 1 = fully occluded). `rayCount` hemisphere
// samples per vertex; `maxDistance` limits how far an occluder counts (so a distant wall doesn't over-darken).
inline std::vector<float> bakeVertexAO(const shapes::MeshData& mesh, int rayCount, float maxDistance) {
    const std::size_t vn = mesh.vertices.size();
    std::vector<float> ao(vn, 0.0f);
    if (mesh.indices.size() < 3 || rayCount < 1 || vn == 0) return ao;

    // Positions + area-weighted smooth normals.
    std::vector<math::vec3> pos(vn), nrm(vn, math::vec3(0, 0, 0));
    for (std::size_t i = 0; i < vn; ++i) pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t ia = mesh.indices[t], ib = mesh.indices[t + 1], ic = mesh.indices[t + 2];
        const math::vec3 fn = math::cross(pos[ib] - pos[ia], pos[ic] - pos[ia]); // area-weighted (unnormalized)
        nrm[ia] += fn; nrm[ib] += fn; nrm[ic] += fn;
    }
    for (math::vec3& n : nrm) {
        const float l = std::sqrt(math::dot(n, n));
        n = l > 1e-12f ? n / l : math::vec3(0, 1, 0);
    }

    const float golden = 2.39996322972865332f; // golden angle (radians)
    for (std::size_t vi = 0; vi < vn; ++vi) {
        const math::vec3 n = nrm[vi];
        // Tangent basis around the normal.
        const math::vec3 ref = (std::fabs(n.y) < 0.99f) ? math::vec3(0, 1, 0) : math::vec3(1, 0, 0);
        const math::vec3 tang = math::normalize(math::cross(ref, n));
        const math::vec3 bit = math::cross(n, tang);
        const math::vec3 origin = pos[vi] + n * 1e-3f; // lift off the surface to avoid self-hit

        int blocked = 0;
        for (int r = 0; r < rayCount; ++r) {
            // Uniform-over-hemisphere direction: cosTheta uniform in (0,1), phi via the golden angle.
            const float cosT = (static_cast<float>(r) + 0.5f) / static_cast<float>(rayCount);
            const float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
            const float phi = static_cast<float>(r) * golden;
            const math::vec3 dir = tang * (std::cos(phi) * sinT) + bit * (std::sin(phi) * sinT) + n * cosT;

            bool hit = false;
            for (std::size_t t = 0; t + 2 < mesh.indices.size() && !hit; t += 3) {
                const std::uint32_t ia = mesh.indices[t], ib = mesh.indices[t + 1], ic = mesh.indices[t + 2];
                if (ia == vi || ib == vi || ic == vi) continue; // skip the vertex's own triangles
                float tt;
                if (detail::aoRayTri(origin, dir, pos[ia], pos[ib], pos[ic], maxDistance, tt)) hit = true;
            }
            if (hit) ++blocked;
        }
        ao[vi] = static_cast<float>(blocked) / static_cast<float>(rayCount);
    }
    return ao;
}

} // namespace maz::render
