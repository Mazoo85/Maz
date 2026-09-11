#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

// maz::render MESH STATISTICS — the at-a-glance size-and-scale report an editor's mesh-info panel or an import
// log shows: the axis-aligned BOUNDING BOX (min/max/size/centre), the area-weighted CENTROID (the surface's
// balance point), the total SURFACE AREA, and the edge-length distribution (shortest/longest/mean — a quick read
// on tessellation uniformity and whether the mesh is scaled sanely). It answers "how big is this thing, where is
// it centred, how dense is it" before you place, scale, texture (texel density needs area), or LOD it. Distinct
// from TriangleQuality (M537, per-triangle SHAPE) and MeshMassProperties (M532, the SOLID's volume/inertia):
// this is the SURFACE's extent and area. Reuses only vec3/cross from maz::math. Header-only, deterministic.
//
// Scope note (honest): the centroid is AREA-weighted over triangles (the balance point of the shell), not the
// vertex average and not the solid's centre of mass (that is MeshMassProperties). Edge stats count each
// undirected edge once. Surface area sums triangle areas as-is (a double-sided or self-overlapping mesh counts
// the overlap twice — accurate to the triangles present).
namespace maz::render {

struct MeshStats {
    std::uint32_t vertexCount = 0;
    std::uint32_t triangleCount = 0;
    std::uint32_t edgeCount = 0;            // distinct undirected edges
    math::vec3 boundsMin{0, 0, 0};
    math::vec3 boundsMax{0, 0, 0};
    math::vec3 centroid{0, 0, 0};           // area-weighted surface centroid
    double surfaceArea = 0.0;
    float minEdgeLength = 0.0f;
    float maxEdgeLength = 0.0f;
    double meanEdgeLength = 0.0;

    math::vec3 size() const { return boundsMax - boundsMin; }
    math::vec3 boundsCenter() const {
        return math::vec3((boundsMin.x + boundsMax.x) * 0.5f, (boundsMin.y + boundsMax.y) * 0.5f,
                          (boundsMin.z + boundsMax.z) * 0.5f);
    }
};

// Compute size/area/edge statistics for `mesh`. An empty mesh yields a zeroed report.
inline MeshStats analyzeMesh(const shapes::MeshData& mesh) {
    MeshStats s;
    s.vertexCount = static_cast<std::uint32_t>(mesh.vertices.size());
    if (mesh.vertices.empty()) return s;

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    // Bounding box over all vertices.
    math::vec3 bmin = pos(0), bmax = pos(0);
    for (std::uint32_t i = 1; i < s.vertexCount; ++i) {
        const math::vec3 p = pos(i);
        bmin = math::vec3(std::min(bmin.x, p.x), std::min(bmin.y, p.y), std::min(bmin.z, p.z));
        bmax = math::vec3(std::max(bmax.x, p.x), std::max(bmax.y, p.y), std::max(bmax.z, p.z));
    }
    s.boundsMin = bmin;
    s.boundsMax = bmax;

    // Per-triangle area, area-weighted centroid, and distinct-edge lengths.
    std::unordered_set<std::uint64_t> seenEdge;
    seenEdge.reserve(mesh.indices.size());
    auto ekey = [](std::uint32_t a, std::uint32_t b) {
        return a < b ? (static_cast<std::uint64_t>(a) << 32) | b : (static_cast<std::uint64_t>(b) << 32) | a;
    };
    double area = 0.0, centX = 0.0, centY = 0.0, centZ = 0.0;
    double edgeSum = 0.0;
    float eMin = 1e30f, eMax = 0.0f;
    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        if (ia >= s.vertexCount || ib >= s.vertexCount || ic >= s.vertexCount) continue;
        const math::vec3 a = pos(ia), b = pos(ib), c = pos(ic);
        const math::vec3 cr = math::cross(b - a, c - a);
        const double triArea = 0.5 * std::sqrt(static_cast<double>(math::dot(cr, cr)));
        area += triArea;
        const double cx = (static_cast<double>(a.x) + b.x + c.x) / 3.0;
        const double cy = (static_cast<double>(a.y) + b.y + c.y) / 3.0;
        const double cz = (static_cast<double>(a.z) + b.z + c.z) / 3.0;
        centX += cx * triArea;
        centY += cy * triArea;
        centZ += cz * triArea;

        const std::uint32_t tri[3] = {ia, ib, ic};
        for (int e = 0; e < 3; ++e) {
            const std::uint32_t u = tri[e], w = tri[(e + 1) % 3];
            if (u == w) continue;
            if (!seenEdge.insert(ekey(u, w)).second) continue;
            const math::vec3 d = pos(w) - pos(u);
            const float len = std::sqrt(math::dot(d, d));
            edgeSum += len;
            if (len < eMin) eMin = len;
            if (len > eMax) eMax = len;
        }
    }
    s.triangleCount = static_cast<std::uint32_t>(triN);
    s.surfaceArea = area;
    if (area > 1e-20) s.centroid = math::vec3(static_cast<float>(centX / area), static_cast<float>(centY / area),
                                              static_cast<float>(centZ / area));
    else s.centroid = s.boundsCenter();

    s.edgeCount = static_cast<std::uint32_t>(seenEdge.size());
    if (s.edgeCount > 0) {
        s.minEdgeLength = eMin;
        s.maxEdgeLength = eMax;
        s.meanEdgeLength = edgeSum / static_cast<double>(s.edgeCount);
    }
    return s;
}

} // namespace maz::render
