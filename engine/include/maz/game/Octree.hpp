#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// maz::game::Octree — a bounded-region octree over 3D axis-aligned boxes: the 3D counterpart of
// game::Quadtree, for broadphase culling and spatial range queries in a 3D world. Dense regions
// subdivide into eight children while empty space stays a single node, so a query only descends
// into the octants that could overlap — the standard structure for frustum/box culling of a large
// 3D scene, physics broadphase candidate lists, and 3D neighbour/AoE queries.
//
// Each item is an id + a 3D AABB (x, y, z, w, h, d). insert() pushes an item to the deepest node
// that still fully contains it (capped by maxDepth); items straddling a child boundary rest at the
// parent. query(region) prunes non-overlapping subtrees and returns an EXACT overlap set (no false
// negatives, and every returned item genuinely overlaps). Header-only, no GPU; rebuild per frame
// for dynamic scenes (clear() + re-insert) or keep for static geometry.
namespace maz::game {

inline bool aabb3Overlap(float ax, float ay, float az, float aw, float ah, float ad, float bx,
                         float by, float bz, float bw, float bh, float bd) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by && az < bz + bd &&
           az + ad > bz;
}

// True if box A (a*) is fully contained within box B (b*), in 3D.
inline bool aabb3Contains(float bx, float by, float bz, float bw, float bh, float bd, float ax,
                          float ay, float az, float aw, float ah, float ad) {
    return ax >= bx && ay >= by && az >= bz && ax + aw <= bx + bw && ay + ah <= by + bh &&
           az + ad <= bz + bd;
}

class Octree {
  public:
    struct Item {
        uint32_t id;
        float x, y, z, w, h, d;
    };

    // Cover the box at (x,y,z) with size (w,h,d). A node subdivides once it holds more than
    // maxItems, until maxDepth.
    Octree(float x, float y, float z, float w, float h, float d, int maxDepth = 8, int maxItems = 8)
        : m_maxDepth(maxDepth < 0 ? 0 : maxDepth), m_maxItems(maxItems < 1 ? 1 : maxItems) {
        m_root = std::make_unique<Node>(x, y, z, w, h, d);
    }

    void insert(uint32_t id, float x, float y, float z, float w, float h, float d) {
        insertInto(*m_root, Item{id, x, y, z, w, h, d}, 0);
        ++m_size;
    }

    void clear() {
        const Node& r = *m_root;
        m_root = std::make_unique<Node>(r.x, r.y, r.z, r.w, r.h, r.d);
        m_size = 0;
    }

    // All item ids whose AABB overlaps the query box. Exact — no false negatives or geometric false
    // positives.
    std::vector<uint32_t> query(float x, float y, float z, float w, float h, float d) const {
        std::vector<uint32_t> out;
        queryNode(*m_root, x, y, z, w, h, d, out);
        return out;
    }

    std::vector<uint32_t> queryPoint(float px, float py, float pz) const {
        return query(px, py, pz, 0.0f, 0.0f, 0.0f);
    }

    // Everything overlapping a sphere's bounding box (a cheap prefilter — caller does the exact
    // sphere test on the returned candidates).
    std::vector<uint32_t> querySphere(float cx, float cy, float cz, float r) const {
        return query(cx - r, cy - r, cz - r, 2 * r, 2 * r, 2 * r);
    }

    size_t size() const { return m_size; }
    int nodeCount() const { return countNodes(*m_root); }

  private:
    struct Node {
        float x, y, z, w, h, d;
        std::vector<Item> items;
        std::array<std::unique_ptr<Node>, 8> kids{};
        Node(float x_, float y_, float z_, float w_, float h_, float d_)
            : x(x_), y(y_), z(z_), w(w_), h(h_), d(d_) {}
        bool leaf() const { return !kids[0]; }
    };

    void insertInto(Node& n, const Item& it, int depth) {
        if (depth < m_maxDepth) {
            if (n.leaf() && n.items.size() >= static_cast<size_t>(m_maxItems)) {
                subdivide(n);
            }
            if (!n.leaf()) {
                const int o = childIndexFor(n, it);
                if (o >= 0) {
                    insertInto(*n.kids[static_cast<size_t>(o)], it, depth + 1);
                    return;
                }
            }
        }
        n.items.push_back(it);
    }

    static void subdivide(Node& n) {
        const float hw = n.w * 0.5f, hh = n.h * 0.5f, hd = n.d * 0.5f;
        // Octant order: bit0 = +x, bit1 = +y, bit2 = +z.
        for (int o = 0; o < 8; ++o) {
            const float ox = n.x + ((o & 1) ? hw : 0.0f);
            const float oy = n.y + ((o & 2) ? hh : 0.0f);
            const float oz = n.z + ((o & 4) ? hd : 0.0f);
            n.kids[static_cast<size_t>(o)] = std::make_unique<Node>(ox, oy, oz, hw, hh, hd);
        }
        std::vector<Item> keep;
        for (const Item& it : n.items) {
            const int o = childIndexFor(n, it);
            if (o >= 0) {
                n.kids[static_cast<size_t>(o)]->items.push_back(it);
            } else {
                keep.push_back(it);
            }
        }
        n.items.swap(keep);
    }

    static int childIndexFor(const Node& n, const Item& it) {
        for (int o = 0; o < 8; ++o) {
            const Node& c = *n.kids[static_cast<size_t>(o)];
            if (aabb3Contains(c.x, c.y, c.z, c.w, c.h, c.d, it.x, it.y, it.z, it.w, it.h, it.d)) {
                return o;
            }
        }
        return -1;
    }

    static void queryNode(const Node& n, float qx, float qy, float qz, float qw, float qh, float qd,
                          std::vector<uint32_t>& out) {
        if (!aabb3Overlap(n.x, n.y, n.z, n.w, n.h, n.d, qx, qy, qz, qw, qh, qd)) {
            return;
        }
        for (const Item& it : n.items) {
            if (aabb3Overlap(it.x, it.y, it.z, it.w, it.h, it.d, qx, qy, qz, qw, qh, qd)) {
                out.push_back(it.id);
            }
        }
        if (!n.leaf()) {
            for (const auto& c : n.kids) {
                queryNode(*c, qx, qy, qz, qw, qh, qd, out);
            }
        }
    }

    static int countNodes(const Node& n) {
        int c = 1;
        if (!n.leaf()) {
            for (const auto& k : n.kids) {
                c += countNodes(*k);
            }
        }
        return c;
    }

    std::unique_ptr<Node> m_root;
    int m_maxDepth;
    int m_maxItems;
    size_t m_size = 0;
};

} // namespace maz::game
