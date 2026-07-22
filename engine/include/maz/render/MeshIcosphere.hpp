#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::render ICOSPHERE / GEODESIC SPHERE — a round ball built from an icosahedron (a 20-sided die) repeatedly
// split into smaller triangles, giving a sphere whose triangles are all nearly the SAME size and shape. This is
// the good kind of sphere for most jobs: the everyday "UV sphere" (M28 makeSphere) crowds its triangles into
// tight pinch-points at the north and south poles, which shows up as ugly stretching on planets, blotchy shading,
// and uneven tessellation — the icosphere has no poles and no pinching, so it lights evenly and subdivides
// cleanly. Reach for it for planets and moons, explosion / shockwave domes, force-field bubbles, evenly-spread
// point scatters, low-poly rock/asteroid bases, and anywhere you want a sphere that looks the same from every
// angle. `subdivisions` controls smoothness: 0 is the raw 20-face icosahedron (a faceted gem), 1 = 80 faces, 2 =
// 320, each level multiplying the face count by 4. Header-only, deterministic, headless — pure CPU geometry.
//
// Scope note (honest): normals are the exact analytic sphere normals (the normalized position), so shading is
// perfectly smooth with no extra `computeNormals` pass. UVs use the standard latitude/longitude mapping, which
// has the usual single wrap SEAM down one side and a slight pinch at the very top/bottom texel (shared by every
// lat-long sphere) — fine for solid colours, procedural/triplanar texturing, or a seam-tolerant map; re-unwrap if
// you need a perfect atlas. `subdivisions` is clamped to >= 0; the mesh is a closed watertight shell.
namespace maz::render {

// A geodesic sphere of the given radius. `subdivisions` (>=0) splits each face into 4 that many times.
inline shapes::MeshData makeIcosphere(float radius, int subdivisions) {
    const int levels = subdivisions < 0 ? 0 : subdivisions;

    // --- 12 icosahedron vertices (golden-ratio rectangles), stored as UNIT directions. ---
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    std::vector<math::vec3> dir = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1},
    };
    for (math::vec3& v : dir) {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        v = math::vec3(v.x / len, v.y / len, v.z / len);
    }
    std::vector<std::uint32_t> idx = {
        0, 11, 5,  0, 5, 1,  0, 1, 7,  0, 7, 10,  0, 10, 11,
        1, 5, 9,  5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
        3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
        4, 9, 5,  2, 4, 11,  6, 2, 10,  8, 6, 7,  9, 8, 1,
    };

    // --- Subdivide: split each triangle into 4, sharing edge midpoints via a cache keyed by the vertex pair. ---
    std::unordered_map<std::uint64_t, std::uint32_t> midCache;
    auto midpoint = [&](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
        const std::uint64_t key = a < b ? (static_cast<std::uint64_t>(a) << 32) | b
                                        : (static_cast<std::uint64_t>(b) << 32) | a;
        auto it = midCache.find(key);
        if (it != midCache.end()) return it->second;
        const math::vec3& pa = dir[a];
        const math::vec3& pb = dir[b];
        math::vec3 m(pa.x + pb.x, pa.y + pb.y, pa.z + pb.z);
        const float len = std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z);
        m = math::vec3(m.x / len, m.y / len, m.z / len); // back onto the unit sphere
        const std::uint32_t id = static_cast<std::uint32_t>(dir.size());
        dir.push_back(m);
        midCache.emplace(key, id);
        return id;
    };
    for (int level = 0; level < levels; ++level) {
        std::vector<std::uint32_t> next;
        next.reserve(idx.size() * 4);
        for (std::size_t f = 0; f < idx.size(); f += 3) {
            const std::uint32_t a = idx[f], b = idx[f + 1], c = idx[f + 2];
            const std::uint32_t ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            next.insert(next.end(), {a, ab, ca,  b, bc, ab,  c, ca, bc,  ab, bc, ca});
        }
        idx.swap(next);
    }

    // --- Assemble: position = dir*radius, normal = dir, lat/long UVs, white colour. ---
    const float pi = 3.14159265358979323846f;
    shapes::MeshData out;
    out.vertices.reserve(dir.size());
    for (const math::vec3& d : dir) {
        MeshVertex v{};
        v.px = d.x * radius; v.py = d.y * radius; v.pz = d.z * radius;
        v.nx = d.x; v.ny = d.y; v.nz = d.z;
        v.r = v.g = v.b = 1.0f;
        v.u = 0.5f + std::atan2(d.z, d.x) / (2.0f * pi);
        v.v = 0.5f - std::asin(d.y) / pi;
        out.vertices.push_back(v);
    }
    out.indices = std::move(idx);
    return out;
}

} // namespace maz::render
