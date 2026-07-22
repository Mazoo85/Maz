#pragma once

#include "maz/math/Math.hpp"                    // math::vec3
#include "maz/render/MeshAmbientOcclusion.hpp"  // detail::aoRayTri (Möller–Trumbore ray/triangle)
#include "maz/render/Shapes.hpp"                // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render POINT-IN-MESH CONTAINMENT — is a point INSIDE a closed triangle mesh? For each query point, cast
// one ray to infinity and count how many triangles it crosses: an odd count means inside, even means outside
// (the Jordan-curve / ray-parity test), reusing the M533 Möller–Trumbore ray/triangle — the same inside test
// that signs MeshSdf (M534) and fills MeshVoxelize (M540). Unlike those, this answers arbitrary points DIRECTLY
// with no grid to bake, so it is the right tool for a handful of ad-hoc tests: is a spawn point inside the
// level geometry, is a particle/agent still within a volume, does a prop's centre sit inside a trigger solid,
// rejection-sampling points into a shape. Batch and single-point entry points; an oblique ray dodges the
// degenerate through-a-shared-edge case. Pure CPU, header-only, headless.
//
// Scope note (honest): brute force O(points · triangles) with a ray-parity sign, so it assumes a watertight
// mesh and suits modest point counts / triangle counts; for many queries against a big mesh, bake a MeshSdf
// once (or index with game::Bvh) instead — the documented faster path. A generalized-winding-number test for
// open meshes is the follow-up shared with MeshSdf.
namespace maz::render {

namespace detail {

// Ray-parity inside test for one point against a triangle list (positions pre-extracted).
inline bool pointInsideTris(const math::vec3& p, const std::vector<math::vec3>& pos,
                            const std::vector<std::uint32_t>& indices, const math::vec3& rayDir, float maxRay) {
    int crossings = 0;
    const std::size_t triN = indices.size();
    for (std::size_t t = 0; t + 2 < triN; t += 3) {
        const math::vec3& a = pos[indices[t]];
        const math::vec3& b = pos[indices[t + 1]];
        const math::vec3& c = pos[indices[t + 2]];
        float tt;
        if (aoRayTri(p, rayDir, a, b, c, maxRay, tt)) ++crossings;
    }
    return (crossings & 1) != 0;
}

} // namespace detail

// True if `p` is inside the closed mesh `mesh`.
inline bool containsPoint(const shapes::MeshData& mesh, const math::vec3& p) {
    if (mesh.indices.size() < 3 || mesh.vertices.empty()) return false;
    std::vector<math::vec3> pos(mesh.vertices.size());
    math::vec3 bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
        bmin = math::vec3(std::min(bmin.x, pos[i].x), std::min(bmin.y, pos[i].y), std::min(bmin.z, pos[i].z));
        bmax = math::vec3(std::max(bmax.x, pos[i].x), std::max(bmax.y, pos[i].y), std::max(bmax.z, pos[i].z));
    }
    const math::vec3 ext = bmax - bmin;
    const float longest = std::max(ext.x, std::max(ext.y, ext.z));
    const math::vec3 rayDir(0.573462f, 0.331298f, 0.748981f); // oblique — avoids axis-aligned edges/vertices
    return detail::pointInsideTris(p, pos, mesh.indices, rayDir, longest * 4.0f + 1.0f);
}

// Batch: test many points at once (extracts positions and the bounding box once). Returns one flag per point.
inline std::vector<std::uint8_t> containsPoints(const shapes::MeshData& mesh,
                                                const std::vector<math::vec3>& points) {
    std::vector<std::uint8_t> out(points.size(), 0);
    if (mesh.indices.size() < 3 || mesh.vertices.empty() || points.empty()) return out;

    std::vector<math::vec3> pos(mesh.vertices.size());
    math::vec3 bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
        bmin = math::vec3(std::min(bmin.x, pos[i].x), std::min(bmin.y, pos[i].y), std::min(bmin.z, pos[i].z));
        bmax = math::vec3(std::max(bmax.x, pos[i].x), std::max(bmax.y, pos[i].y), std::max(bmax.z, pos[i].z));
    }
    const math::vec3 ext = bmax - bmin;
    const float longest = std::max(ext.x, std::max(ext.y, ext.z));
    const float maxRay = longest * 4.0f + 1.0f;
    const math::vec3 rayDir(0.573462f, 0.331298f, 0.748981f);

    for (std::size_t i = 0; i < points.size(); ++i) {
        const math::vec3& p = points[i];
        // Quick reject: a point outside the bounding box is definitely outside.
        if (p.x < bmin.x || p.y < bmin.y || p.z < bmin.z || p.x > bmax.x || p.y > bmax.y || p.z > bmax.z)
            continue;
        out[i] = detail::pointInsideTris(p, pos, mesh.indices, rayDir, maxRay) ? 1u : 0u;
    }
    return out;
}

} // namespace maz::render
