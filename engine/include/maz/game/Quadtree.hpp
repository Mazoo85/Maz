#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// maz::game::Quadtree — a bounded-region quadtree over 2D axis-aligned boxes, for broadphase
// culling and spatial range queries. Where the uniform spatial grid (game::SpatialGrid) is ideal
// for evenly-spread objects, a quadtree adapts to CLUSTERED distributions: dense regions subdivide
// deep while empty space stays a single big node, so a query only visits the cells that could
// possibly overlap. This is the structure engines (Godot included) reach for to answer "what's near
// here?" — visible-set culling, mouse/AoE picking, neighbour finding — in better-than-linear time.
//
// Each item is an id + an AABB (x, y, w, h). insert() pushes an item down to the deepest node that
// still fully contains it (capped by maxDepth); items straddling a child boundary rest at the
// parent. query(region) walks only nodes whose bounds overlap the region and returns every item
// whose AABB overlaps it — no false negatives (a superset-free exact overlap set), with
// non-overlapping subtrees pruned. Header-only, no GPU; rebuilt per frame for dynamic scenes
// (clear() + re-insert) or kept for static geometry.
namespace maz::game {

inline bool aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                        float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// True if box A (a*) is fully contained within box B (b*).
inline bool aabbContains(float bx, float by, float bw, float bh, float ax, float ay, float aw,
                         float ah) {
    return ax >= bx && ay >= by && ax + aw <= bx + bw && ay + ah <= by + bh;
}

class Quadtree {
  public:
    struct Item {
        uint32_t id;
        float x, y, w, h;
    };

    // Cover [x, x+w] x [y, y+h]. A node subdivides once it holds more than maxItems, until
    // maxDepth.
    Quadtree(float x, float y, float w, float h, int maxDepth = 8, int maxItems = 8)
        : m_maxDepth(maxDepth < 0 ? 0 : maxDepth), m_maxItems(maxItems < 1 ? 1 : maxItems) {
        m_root = std::make_unique<Node>(x, y, w, h);
    }

    void insert(uint32_t id, float x, float y, float w, float h) {
        insertInto(*m_root, Item{id, x, y, w, h}, 0);
        ++m_size;
    }

    void clear() {
        const Node& r = *m_root;
        m_root = std::make_unique<Node>(r.x, r.y, r.w, r.h);
        m_size = 0;
    }

    // All item ids whose AABB overlaps the query region. Exact (no false negatives, no false
    // positives from geometry — every returned item genuinely overlaps).
    std::vector<uint32_t> query(float x, float y, float w, float h) const {
        std::vector<uint32_t> out;
        queryNode(*m_root, x, y, w, h, out);
        return out;
    }

    // Convenience: everything overlapping a single point.
    std::vector<uint32_t> queryPoint(float px, float py) const { return query(px, py, 0.0f, 0.0f); }

    // Convenience: everything overlapping a circle's bounding box (a cheap prefilter — the caller
    // does the exact circle test on the returned candidates).
    std::vector<uint32_t> queryCircle(float cx, float cy, float r) const {
        return query(cx - r, cy - r, 2.0f * r, 2.0f * r);
    }

    size_t size() const { return m_size; }
    int nodeCount() const { return countNodes(*m_root); }

  private:
    struct Node {
        float x, y, w, h;
        std::vector<Item> items;
        std::array<std::unique_ptr<Node>, 4> kids{}; // NW, NE, SW, SE (null until subdivided)
        Node(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
        bool leaf() const { return !kids[0]; }
    };

    void insertInto(Node& n, const Item& it, int depth) {
        // Descend into the single child that fully contains the item, if we may still subdivide.
        if (depth < m_maxDepth) {
            if (n.leaf() && n.items.size() >= static_cast<size_t>(m_maxItems)) {
                subdivide(n);
            }
            if (!n.leaf()) {
                const int q = childIndexFor(n, it);
                if (q >= 0) {
                    insertInto(*n.kids[static_cast<size_t>(q)], it, depth + 1);
                    return;
                }
            }
        }
        n.items.push_back(it); // straddles a boundary, or at max depth -> rest here
    }

    static void subdivide(Node& n) {
        const float hw = n.w * 0.5f, hh = n.h * 0.5f;
        n.kids[0] = std::make_unique<Node>(n.x, n.y, hw, hh);           // NW
        n.kids[1] = std::make_unique<Node>(n.x + hw, n.y, hw, hh);      // NE
        n.kids[2] = std::make_unique<Node>(n.x, n.y + hh, hw, hh);      // SW
        n.kids[3] = std::make_unique<Node>(n.x + hw, n.y + hh, hw, hh); // SE
        // Re-home the items that now fit entirely in one child; keep the rest at this node.
        std::vector<Item> keep;
        for (const Item& it : n.items) {
            const int q = childIndexFor(n, it);
            if (q >= 0) {
                n.kids[static_cast<size_t>(q)]->items.push_back(it);
            } else {
                keep.push_back(it);
            }
        }
        n.items.swap(keep);
    }

    // Which child (0..3) fully contains the item, or -1 if it straddles a boundary.
    static int childIndexFor(const Node& n, const Item& it) {
        for (int q = 0; q < 4; ++q) {
            const Node& c = *n.kids[static_cast<size_t>(q)];
            if (aabbContains(c.x, c.y, c.w, c.h, it.x, it.y, it.w, it.h)) {
                return q;
            }
        }
        return -1;
    }

    static void queryNode(const Node& n, float qx, float qy, float qw, float qh,
                          std::vector<uint32_t>& out) {
        if (!aabbOverlap(n.x, n.y, n.w, n.h, qx, qy, qw, qh)) {
            return; // whole subtree is outside the query
        }
        for (const Item& it : n.items) {
            if (aabbOverlap(it.x, it.y, it.w, it.h, qx, qy, qw, qh)) {
                out.push_back(it.id);
            }
        }
        if (!n.leaf()) {
            for (const auto& c : n.kids) {
                queryNode(*c, qx, qy, qw, qh, out);
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
