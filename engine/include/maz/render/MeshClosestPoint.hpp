#pragma once

#include "maz/math/Geometry3D.hpp" // math::closestPointOnTriangle
#include "maz/math/Math.hpp"        // math::vec3
#include "maz/render/Shapes.hpp"    // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>

// maz::render CLOSEST POINT ON A MESH — for any point in space, find the nearest spot ON the model's surface and
// how far away it is. This is the "snap to surface" / "how deep am I" query games lean on constantly: stick a
// decal, bullet-hole, or footprint flat on the wall it hit; snap a placed object or the mouse cursor onto the
// terrain; find how far a character has sunk into geometry to push them back out; measure clearance to the nearest
// wall; pick the mesh vertex/face closest to a click. Unlike a raycast (which needs a direction and can miss), this
// always returns an answer — the single closest surface point, no matter where the query point sits. It walks every
// triangle with the engine's exact `closestPointOnTriangle` and keeps the nearest, also reporting which triangle
// won and that triangle's facing normal (handy for orienting a decal). Header-only, deterministic, headless.
//
// Scope note (honest): this is a brute-force scan over all triangles — O(triangles) per query, ideal for one-off
// queries and small/medium meshes; for many queries against a big mesh, put a BVH (game::Bvh / TriMesh3D) in front.
// It returns the closest point on the SURFACE and an UNSIGNED distance — it does not say inside vs outside (use the
// M556 containment / M551 SDF tools for a signed result). `normal` is the hit triangle's geometric face normal
// (normalized; zero for a degenerate triangle). An empty mesh yields `valid == false`.
namespace maz::render {

struct ClosestPointResult {
    bool valid = false;
    math::vec3 point{0, 0, 0};  // nearest point on the mesh surface
    float distance = 0.0f;      // unsigned distance from the query to that point
    int triangle = -1;          // index of the winning triangle
    math::vec3 normal{0, 0, 0}; // that triangle's unit geometric normal
};

// Find the point on `mesh` closest to `query`. Scans all triangles; returns the nearest hit.
inline ClosestPointResult closestPointOnMesh(const shapes::MeshData& mesh, const math::vec3& query) {
    ClosestPointResult out;
    const std::size_t triN = mesh.indices.size() / 3;
    if (triN == 0 || mesh.vertices.empty()) return out;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    float best = 0.0f;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) continue;
        const math::vec3 a = pos(ia), b = pos(ib), c = pos(ic);
        const math::vec3 cp = math::closestPointOnTriangle(query, a, b, c);
        const math::vec3 d(cp.x - query.x, cp.y - query.y, cp.z - query.z);
        const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (!out.valid || dist2 < best) {
            best = dist2;
            out.valid = true;
            out.point = cp;
            out.triangle = static_cast<int>(t);
            // Geometric face normal (normalized; zero for a degenerate triangle).
            const math::vec3 n = math::cross(b - a, c - a);
            const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            out.normal = len > 1e-20f ? math::vec3(n.x / len, n.y / len, n.z / len) : math::vec3(0, 0, 0);
        }
    }
    if (out.valid) out.distance = std::sqrt(best);
    return out;
}

} // namespace maz::render
