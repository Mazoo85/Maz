#pragma once

#include "maz/math/TriangleIntersect.hpp" // math::trianglesIntersect
#include "maz/render/Shapes.hpp"          // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render MESH SELF-INTERSECTION DETECTION — find faces of a mesh that poke through OTHER faces of the same
// mesh, the "select self-intersecting" mesh-repair pass in Blender/Godot. Self-intersections are a common defect
// that breaks 3D printing (non-manifold solid), boolean/CSG operations, physics collision, and clean shading; a
// modeller wants them flagged. Built on the M549 Möller triangle-triangle test: for every pair of triangles that
// do NOT share a vertex (edge/vertex-adjacent faces legitimately touch and must be excluded), quick-reject by
// their axis-aligned boxes, then run the exact tri-tri test and record the crossing pair. Reports pairs with
// triA < triB, in ascending order, so the output is deterministic. Pure CPU, header-only, headless.
//
// Scope note (honest): brute-force O(triangles^2) with an AABB reject — ideal for the offline QA of props and
// modest meshes; for a very large mesh, front it with a broadphase (a sweep or the M547 BVH boxes) to avoid the
// quadratic pair loop, the documented follow-up. "Adjacent" means sharing a vertex INDEX, so an UNWELDED mesh
// with duplicate coincident vertices can report touching faces as intersecting — run MeshWeld first. Touching
// (a graze) counts as intersecting, matching the underlying tri-tri test.
namespace maz::render {

// One detected self-intersection: the two triangle indices that cross (triA < triB).
struct SelfIntersection {
    std::uint32_t triA = 0;
    std::uint32_t triB = 0;
};

// All pairs of non-adjacent triangles in `mesh` that intersect. Empty if the mesh is clean (or has < 2 triangles).
inline std::vector<SelfIntersection> findSelfIntersections(const shapes::MeshData& mesh) {
    std::vector<SelfIntersection> out;
    const std::size_t triN = mesh.indices.size() / 3;
    const std::size_t vn = mesh.vertices.size();
    if (triN < 2 || vn == 0) return out;

    // Pre-extract each triangle's three vertex indices, corner positions, and AABB.
    struct Tri {
        std::uint32_t i0, i1, i2;
        math::vec3 a, b, c;
        math::vec3 lo, hi;
    };
    std::vector<Tri> tris(triN);
    for (std::size_t t = 0; t < triN; ++t) {
        Tri& tr = tris[t];
        tr.i0 = mesh.indices[3 * t];
        tr.i1 = mesh.indices[3 * t + 1];
        tr.i2 = mesh.indices[3 * t + 2];
        auto pos = [&](std::uint32_t i) {
            const MeshVertex& v = mesh.vertices[i < vn ? i : 0];
            return math::vec3(v.px, v.py, v.pz);
        };
        tr.a = pos(tr.i0);
        tr.b = pos(tr.i1);
        tr.c = pos(tr.i2);
        tr.lo = math::vec3(std::min(tr.a.x, std::min(tr.b.x, tr.c.x)), std::min(tr.a.y, std::min(tr.b.y, tr.c.y)),
                           std::min(tr.a.z, std::min(tr.b.z, tr.c.z)));
        tr.hi = math::vec3(std::max(tr.a.x, std::max(tr.b.x, tr.c.x)), std::max(tr.a.y, std::max(tr.b.y, tr.c.y)),
                           std::max(tr.a.z, std::max(tr.b.z, tr.c.z)));
    }

    auto shareVertex = [](const Tri& x, const Tri& y) {
        const std::uint32_t xi[3] = {x.i0, x.i1, x.i2};
        const std::uint32_t yi[3] = {y.i0, y.i1, y.i2};
        for (int p = 0; p < 3; ++p)
            for (int q = 0; q < 3; ++q)
                if (xi[p] == yi[q]) return true;
        return false;
    };
    auto boxesOverlap = [](const Tri& x, const Tri& y) {
        return x.lo.x <= y.hi.x && x.hi.x >= y.lo.x && x.lo.y <= y.hi.y && x.hi.y >= y.lo.y && x.lo.z <= y.hi.z &&
               x.hi.z >= y.lo.z;
    };

    for (std::size_t i = 0; i < triN; ++i) {
        for (std::size_t j = i + 1; j < triN; ++j) {
            const Tri& x = tris[i];
            const Tri& y = tris[j];
            if (shareVertex(x, y)) continue;  // edge/vertex-adjacent faces legitimately touch
            if (!boxesOverlap(x, y)) continue; // cheap reject before the exact test
            if (math::trianglesIntersect(x.a, x.b, x.c, y.a, y.b, y.c))
                out.push_back({static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
        }
    }
    return out;
}

// Convenience: does the mesh self-intersect at all? Stops at the first crossing pair found.
inline bool hasSelfIntersection(const shapes::MeshData& mesh) {
    const std::size_t triN = mesh.indices.size() / 3;
    const std::size_t vn = mesh.vertices.size();
    if (triN < 2 || vn == 0) return false;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i < vn ? i : 0];
        return math::vec3(v.px, v.py, v.pz);
    };
    for (std::size_t i = 0; i < triN; ++i) {
        const std::uint32_t a0 = mesh.indices[3 * i], a1 = mesh.indices[3 * i + 1], a2 = mesh.indices[3 * i + 2];
        const math::vec3 av0 = pos(a0), av1 = pos(a1), av2 = pos(a2);
        for (std::size_t j = i + 1; j < triN; ++j) {
            const std::uint32_t b0 = mesh.indices[3 * j], b1 = mesh.indices[3 * j + 1], b2 = mesh.indices[3 * j + 2];
            if (a0 == b0 || a0 == b1 || a0 == b2 || a1 == b0 || a1 == b1 || a1 == b2 || a2 == b0 || a2 == b1 ||
                a2 == b2)
                continue;
            if (math::trianglesIntersect(av0, av1, av2, pos(b0), pos(b1), pos(b2))) return true;
        }
    }
    return false;
}

} // namespace maz::render
