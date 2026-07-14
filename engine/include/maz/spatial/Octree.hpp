#pragma once

#include <cstddef>
#include <vector>
#include <memory>   // std::unique_ptr
#include <array>
#include <utility>  // std::move

#include "maz/math/Geometry.hpp"   // maz::math::Aabb, maz::math::vec3
#include "maz/math/Collision.hpp"  // maz::math::Sphere, maz::math::sphereAabb
#include "maz/core/Assert.hpp"     // MAZ_ASSERT

// A hierarchical adaptive point octree over a fixed root AABB — points (each with a
// payload T) are stored in leaves that subdivide into 8 octants once a leaf exceeds
// maxPerNode, bounded by maxDepth (coincident points beyond capacity at max depth
// pile up in the deepest leaf, so subdivision always terminates). queryAabb /
// querySphere descend only into the octants overlapping the query — pruning via
// Aabb intersection / iter36 sphereAabb — then narrow-test each candidate point.
//
// Composes maz::math::Aabb + Collision. Complements iter24 SpatialHashGrid (uniform)
// with an adaptive partition. Inserted points must lie within the root bounds. NOT
// thread-safe. Dynamic removal/relocation and a loose/expanding-root variant are
// future refinements (not built here).

namespace maz::spatial {

using maz::math::Aabb;
using maz::math::vec3;

template <typename T>
class Octree {
  public:
    explicit Octree(const Aabb& bounds, std::size_t maxPerNode = 8, std::size_t maxDepth = 8)
        : m_bounds(bounds), m_maxPerNode(maxPerNode), m_maxDepth(maxDepth) {
        MAZ_ASSERT(bounds.isValid(), "Octree: bounds must be valid (min <= max)");
        MAZ_ASSERT(maxPerNode >= 1, "Octree: maxPerNode must be >= 1");
        m_root = std::make_unique<Node>();
        m_root->bounds = m_bounds;
    }

    void insert(vec3 point, T payload) {
        MAZ_ASSERT(m_bounds.contains(point), "Octree::insert: point outside root bounds");
        insertInto(*m_root, point, std::move(payload), 0);
        ++m_size;
    }

    // Descend into octants overlapping region, then keep points region contains.
    // out is APPENDED to (caller may reuse a buffer) — it is NOT cleared here.
    void queryAabb(const Aabb& region, std::vector<T>& out) const {
        queryAabbInto(*m_root, region, out);
    }

    // Descend into octants overlapping the sphere (iter36 sphereAabb pruning), then
    // keep points within radius (inclusive: on-surface counts). out is APPENDED to.
    void querySphere(vec3 center, float radius, std::vector<T>& out) const {
        querySphereInto(*m_root, center, radius, out);
    }

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    void clear() {
        m_root = std::make_unique<Node>();
        m_root->bounds = m_bounds;
        m_size = 0;
    }

    const Aabb& bounds() const { return m_bounds; }

  private:
    struct Entry {
        vec3 point;
        T payload;
    };

    struct Node {
        Aabb bounds;
        std::vector<Entry> entries;
        std::array<std::unique_ptr<Node>, 8> children{};
        bool leaf = true;
    };

    // Octant index for p within b: a point on a split plane goes to the upper octant
    // (consistent so redistribution and query agree).
    static int octantOf(const Aabb& b, vec3 p) {
        const vec3 c = b.center();
        int o = 0;
        if (p.x >= c.x) { o |= 1; }
        if (p.y >= c.y) { o |= 2; }
        if (p.z >= c.z) { o |= 4; }
        return o;
    }

    // The half-box for that octant, consistent with octantOf so that for any point p
    // in b, childBounds(b, octantOf(b, p)) contains p (redistribution loses no point).
    static Aabb childBounds(const Aabb& b, int octant) {
        const vec3 c = b.center();
        vec3 lo = b.min;
        vec3 hi = b.max;
        if (octant & 1) { lo.x = c.x; } else { hi.x = c.x; }
        if (octant & 2) { lo.y = c.y; } else { hi.y = c.y; }
        if (octant & 4) { lo.z = c.z; } else { hi.z = c.z; }
        return Aabb{lo, hi};
    }

    void insertInto(Node& n, vec3 p, T payload, std::size_t depth) {
        if (n.leaf) {
            // Room to spare, or at the depth cap (never subdivide past maxDepth —
            // coincident points beyond capacity simply pile up in the deepest leaf).
            if (n.entries.size() < m_maxPerNode || depth >= m_maxDepth) {
                n.entries.push_back(Entry{p, std::move(payload)});
                return;
            }
            // Subdivide: create the 8 octant children, then redistribute existing
            // entries down and fall through to insert the new point.
            for (int o = 0; o < 8; ++o) {
                n.children[static_cast<std::size_t>(o)] = std::make_unique<Node>();
                n.children[static_cast<std::size_t>(o)]->bounds = childBounds(n.bounds, o);
            }
            n.leaf = false;
            for (Entry& e : n.entries) {
                const int o = octantOf(n.bounds, e.point);
                Node& child = *n.children[static_cast<std::size_t>(o)];
                insertInto(child, e.point, std::move(e.payload), depth + 1);
            }
            n.entries.clear();
        }
        const int o = octantOf(n.bounds, p);
        Node& child = *n.children[static_cast<std::size_t>(o)];
        insertInto(child, p, std::move(payload), depth + 1);
    }

    static void queryAabbInto(const Node& n, const Aabb& region, std::vector<T>& out) {
        if (!n.bounds.intersects(region)) {
            return;
        }
        if (n.leaf) {
            for (const Entry& e : n.entries) {
                if (region.contains(e.point)) {
                    out.push_back(e.payload);
                }
            }
            return;
        }
        for (const std::unique_ptr<Node>& child : n.children) {
            if (child) {
                queryAabbInto(*child, region, out);
            }
        }
    }

    static void querySphereInto(const Node& n, vec3 center, float radius, std::vector<T>& out) {
        if (!maz::math::sphereAabb(maz::math::Sphere{center, radius}, n.bounds)) {
            return;
        }
        if (n.leaf) {
            for (const Entry& e : n.entries) {
                const vec3 d = e.point - center;
                if (maz::math::dot(d, d) <= radius * radius) {
                    out.push_back(e.payload);
                }
            }
            return;
        }
        for (const std::unique_ptr<Node>& child : n.children) {
            if (child) {
                querySphereInto(*child, center, radius, out);
            }
        }
    }

    std::unique_ptr<Node> m_root;
    Aabb m_bounds;
    std::size_t m_maxPerNode;
    std::size_t m_maxDepth;
    std::size_t m_size = 0;
};

} // namespace maz::spatial
