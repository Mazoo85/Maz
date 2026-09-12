#pragma once

#include "maz/game/Bvh.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// maz::game concave trimesh collider — Godot's ConcavePolygonShape3D: an arbitrary triangle soup
// used as static level geometry (the whole world's collision mesh). Unlike a convex shape it can be
// any shape, so it's a static/immovable collider. Ray queries against a big mesh would be O(triangles)
// naively; TriMesh3D builds a BVH over the per-triangle AABBs and, on a raycast, only tests the few
// triangles whose bounds the ray actually enters (Möller-Trumbore), returning the nearest hit with
// its point, distance, triangle index, and geometric normal. Building on the existing game::Bvh keeps
// this small and exact. Pure math (no renderer/physics), deterministic, header-only, unit-tested.
namespace maz::game {

struct TriMeshHit {
    bool hit = false;
    float t = 0.0f;         // distance along dir
    math::vec3 point{0, 0, 0};
    math::vec3 normal{0, 0, 0}; // geometric face normal (unnormalized-safe: returned normalized)
    int triangle = -1;      // index of the hit triangle
};

class TriMesh3D {
  public:
    // Build from a flat vertex list + triangle indices (3 per triangle).
    void build(const std::vector<math::vec3>& vertices, const std::vector<uint32_t>& indices) {
        m_verts = vertices;
        m_indices = indices;
        m_triCount = static_cast<int>(indices.size() / 3);

        std::vector<Bvh::Item> items;
        items.reserve(static_cast<std::size_t>(m_triCount));
        for (int tri = 0; tri < m_triCount; ++tri) {
            const math::vec3& a = vert(m_indices[static_cast<std::size_t>(tri * 3 + 0)]);
            const math::vec3& b = vert(m_indices[static_cast<std::size_t>(tri * 3 + 1)]);
            const math::vec3& c = vert(m_indices[static_cast<std::size_t>(tri * 3 + 2)]);
            Bvh::Item it;
            it.id = static_cast<uint32_t>(tri);
            it.min[0] = min3(a.x, b.x, c.x);
            it.min[1] = min3(a.y, b.y, c.y);
            it.min[2] = min3(a.z, b.z, c.z);
            it.max[0] = max3(a.x, b.x, c.x);
            it.max[1] = max3(a.y, b.y, c.y);
            it.max[2] = max3(a.z, b.z, c.z);
            items.push_back(it);
        }
        m_bvh.build(items);
    }

    int triangleCount() const { return m_triCount; }

    // Nearest ray hit within [0, maxDist] (in units of |dir|; dir need not be normalized).
    TriMeshHit raycast(const math::vec3& origin, const math::vec3& dir, float maxDist) const {
        TriMeshHit out;
        if (m_triCount == 0) {
            return out;
        }
        const float o[3] = {origin.x, origin.y, origin.z};
        const float d[3] = {dir.x, dir.y, dir.z};
        const std::vector<uint32_t> cand = m_bvh.raycast(o, d, maxDist);

        float best = maxDist;
        for (uint32_t tri : cand) {
            const std::size_t base = static_cast<std::size_t>(tri) * 3;
            const math::vec3& a = vert(m_indices[base + 0]);
            const math::vec3& b = vert(m_indices[base + 1]);
            const math::vec3& c = vert(m_indices[base + 2]);
            float t = 0.0f;
            if (triRay(origin, dir, a, b, c, t) && t < best) {
                best = t;
                out.hit = true;
                out.t = t;
                out.triangle = static_cast<int>(tri);
                out.point = origin + dir * t;
                math::vec3 n = cross3(b - a, c - a);
                const float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                out.normal = l > 0.0f ? math::vec3(n.x / l, n.y / l, n.z / l) : math::vec3(0, 1, 0);
            }
        }
        return out;
    }

  private:
    const math::vec3& vert(uint32_t i) const { return m_verts[static_cast<std::size_t>(i)]; }
    static float min3(float a, float b, float c) {
        float m = a < b ? a : b;
        return m < c ? m : c;
    }
    static float max3(float a, float b, float c) {
        float m = a > b ? a : b;
        return m > c ? m : c;
    }
    static math::vec3 cross3(const math::vec3& a, const math::vec3& b) {
        return math::vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }

    static bool triRay(const math::vec3& o, const math::vec3& d, const math::vec3& a,
                       const math::vec3& b, const math::vec3& c, float& outT) {
        const math::vec3 e1(b.x - a.x, b.y - a.y, b.z - a.z);
        const math::vec3 e2(c.x - a.x, c.y - a.y, c.z - a.z);
        const math::vec3 pv(d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z,
                            d.x * e2.y - d.y * e2.x);
        const float det = e1.x * pv.x + e1.y * pv.y + e1.z * pv.z;
        if (std::fabs(det) < 1e-12f) {
            return false;
        }
        const float inv = 1.0f / det;
        const math::vec3 tv(o.x - a.x, o.y - a.y, o.z - a.z);
        const float u = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
        if (u < -1e-5f || u > 1.0f + 1e-5f) {
            return false;
        }
        const math::vec3 qv(tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z,
                            tv.x * e1.y - tv.y * e1.x);
        const float v = (d.x * qv.x + d.y * qv.y + d.z * qv.z) * inv;
        if (v < -1e-5f || u + v > 1.0f + 1e-5f) {
            return false;
        }
        const float t = (e2.x * qv.x + e2.y * qv.y + e2.z * qv.z) * inv;
        if (t < 0.0f) {
            return false;
        }
        outT = t;
        return true;
    }

    std::vector<math::vec3> m_verts;
    std::vector<uint32_t> m_indices;
    int m_triCount = 0;
    Bvh m_bvh;
};

} // namespace maz::game
