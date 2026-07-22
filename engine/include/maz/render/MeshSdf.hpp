#pragma once

#include "maz/math/Math.hpp"                    // math::vec3, dot, cross
#include "maz/render/MeshAmbientOcclusion.hpp"  // detail::aoRayTri (Möller–Trumbore)
#include "maz/render/Shapes.hpp"                // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render SIGNED DISTANCE FIELD bake — sample the signed distance to a closed mesh's surface onto a 3D
// grid: NEGATIVE inside the solid, POSITIVE outside, ~0 on the surface. An SDF is the shared currency behind
// a lot of engine tech — soft/contact shadows, ambient occlusion, collision & penetration depth, smooth
// CSG/booleans, raymarched volumes, and flow-field navigation around obstacles — and Godot's SDFGI / 2D
// SDF collision use exactly this. The unsigned distance at each grid point is the minimum distance to any
// triangle (exact closest-point-on-triangle); the SIGN comes from a ray-parity inside/outside test (odd
// crossings = inside), reusing the M533 Möller–Trumbore ray/triangle. `sampleMeshSdf` trilinearly
// interpolates the grid at an arbitrary point. Pure CPU, header-only, headless — verifiable against a cube's
// closed-form distance.
//
// Scope note (honest): brute-force O(cells · tris) with a ray-parity sign (assumes a watertight mesh) — fine
// for offline bakes of props/levels; a BVH (game::Bvh) to accelerate the nearest-triangle search and a
// generalized-winding-number sign for open meshes are the documented follow-ups.
namespace maz::render {

struct MeshSdf {
    int nx = 0, ny = 0, nz = 0;
    math::vec3 origin{0, 0, 0}; // world position of grid sample (0,0,0)
    float cellSize = 0.0f;      // uniform spacing between samples
    std::vector<float> dist;    // signed distance, index = (z*ny + y)*nx + x

    float at(int x, int y, int z) const {
        return dist[(static_cast<std::size_t>(z) * static_cast<std::size_t>(ny)
                     + static_cast<std::size_t>(y)) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(x)];
    }
};

namespace detail {

// Closest point on triangle abc to p (Ericson, Real-Time Collision Detection).
inline math::vec3 closestOnTriangle(const math::vec3& p, const math::vec3& a, const math::vec3& b,
                                    const math::vec3& c) {
    const math::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = math::dot(ab, ap), d2 = math::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    const math::vec3 bp = p - b;
    const float d3 = math::dot(ab, bp), d4 = math::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return a + ab * (d1 / (d1 - d3));
    const math::vec3 cp = p - c;
    const float d5 = math::dot(ab, cp), d6 = math::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float denom = 1.0f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

} // namespace detail

// Bake the signed distance field of `mesh` onto a grid over its bounding box expanded by `padding`, with
// `resolution` samples along the longest axis (>= 2). Empty/degenerate mesh yields an empty field.
inline MeshSdf bakeMeshSdf(const shapes::MeshData& mesh, int resolution, float padding) {
    MeshSdf sdf;
    if (mesh.indices.size() < 3 || mesh.vertices.empty() || resolution < 2) return sdf;

    std::vector<math::vec3> pos(mesh.vertices.size());
    math::vec3 bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
        bmin = math::vec3(std::min(bmin.x, pos[i].x), std::min(bmin.y, pos[i].y), std::min(bmin.z, pos[i].z));
        bmax = math::vec3(std::max(bmax.x, pos[i].x), std::max(bmax.y, pos[i].y), std::max(bmax.z, pos[i].z));
    }
    bmin -= math::vec3(padding, padding, padding);
    bmax += math::vec3(padding, padding, padding);
    const math::vec3 ext = bmax - bmin;
    const float longest = std::max(ext.x, std::max(ext.y, ext.z));
    const float cell = longest / static_cast<float>(resolution - 1);
    if (cell <= 0.0f) return sdf;

    sdf.origin = bmin;
    sdf.cellSize = cell;
    sdf.nx = std::max(2, static_cast<int>(ext.x / cell + 0.5f) + 1);
    sdf.ny = std::max(2, static_cast<int>(ext.y / cell + 0.5f) + 1);
    sdf.nz = std::max(2, static_cast<int>(ext.z / cell + 0.5f) + 1);
    sdf.dist.resize(static_cast<std::size_t>(sdf.nx) * static_cast<std::size_t>(sdf.ny)
                    * static_cast<std::size_t>(sdf.nz));

    const std::size_t triN = mesh.indices.size();
    const math::vec3 rayDir = math::vec3(0.573462f, 0.331298f, 0.748981f); // oblique, avoids axis-aligned edges
    const float maxRay = longest * 4.0f + 1.0f;

    for (int z = 0; z < sdf.nz; ++z)
        for (int y = 0; y < sdf.ny; ++y)
            for (int x = 0; x < sdf.nx; ++x) {
                const math::vec3 p = sdf.origin + math::vec3(static_cast<float>(x), static_cast<float>(y),
                                                             static_cast<float>(z)) * cell;
                float best = 1e30f;
                int crossings = 0;
                for (std::size_t t = 0; t + 2 < triN; t += 3) {
                    const math::vec3& a = pos[mesh.indices[t]];
                    const math::vec3& b = pos[mesh.indices[t + 1]];
                    const math::vec3& c = pos[mesh.indices[t + 2]];
                    const math::vec3 cp = detail::closestOnTriangle(p, a, b, c);
                    const math::vec3 dv = p - cp;
                    best = std::min(best, math::dot(dv, dv));
                    float tt;
                    if (detail::aoRayTri(p, rayDir, a, b, c, maxRay, tt)) ++crossings;
                }
                const float d = std::sqrt(best);
                const std::size_t idx = (static_cast<std::size_t>(z) * static_cast<std::size_t>(sdf.ny)
                                         + static_cast<std::size_t>(y)) * static_cast<std::size_t>(sdf.nx)
                                        + static_cast<std::size_t>(x);
                sdf.dist[idx] = (crossings & 1) ? -d : d; // odd crossings => inside => negative
            }
    return sdf;
}

// Trilinearly sample the field at world point `p` (clamped to the grid bounds).
inline float sampleMeshSdf(const MeshSdf& sdf, const math::vec3& p) {
    if (sdf.dist.empty()) return 0.0f;
    auto clampf = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    const float gx = clampf((p.x - sdf.origin.x) / sdf.cellSize, 0.0f, static_cast<float>(sdf.nx - 1));
    const float gy = clampf((p.y - sdf.origin.y) / sdf.cellSize, 0.0f, static_cast<float>(sdf.ny - 1));
    const float gz = clampf((p.z - sdf.origin.z) / sdf.cellSize, 0.0f, static_cast<float>(sdf.nz - 1));
    const int x0 = static_cast<int>(gx), y0 = static_cast<int>(gy), z0 = static_cast<int>(gz);
    const int x1 = std::min(x0 + 1, sdf.nx - 1), y1 = std::min(y0 + 1, sdf.ny - 1), z1 = std::min(z0 + 1, sdf.nz - 1);
    const float fx = gx - static_cast<float>(x0), fy = gy - static_cast<float>(y0), fz = gz - static_cast<float>(z0);
    auto lerp = [](float u, float v, float w) { return u + (v - u) * w; };
    const float c00 = lerp(sdf.at(x0, y0, z0), sdf.at(x1, y0, z0), fx);
    const float c10 = lerp(sdf.at(x0, y1, z0), sdf.at(x1, y1, z0), fx);
    const float c01 = lerp(sdf.at(x0, y0, z1), sdf.at(x1, y0, z1), fx);
    const float c11 = lerp(sdf.at(x0, y1, z1), sdf.at(x1, y1, z1), fx);
    return lerp(lerp(c00, c10, fy), lerp(c01, c11, fy), fz);
}

} // namespace maz::render
