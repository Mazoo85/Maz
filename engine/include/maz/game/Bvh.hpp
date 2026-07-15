#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

// maz::game::Bvh — a bounding-volume hierarchy over 3D AABBs, the acceleration structure engines
// build for RAY CASTING and broadphase over (mostly static) geometry. Where a quad/octree
// partitions space, a BVH partitions the OBJECTS: each node bounds a group of items and splits them
// into two child groups, so a ray or box query descends only the branches its path actually
// touches. This is what makes "what does this ray hit?" — bullet/line-of-sight/mouse-pick against a
// whole level — sublinear instead of testing every triangle-box.
//
// build() takes id+AABB items and recursively splits by the longest axis at the centroid median.
// queryBox() returns every item overlapping a box; raycast() returns every item the ray enters
// (within tMax); raycastNearest() returns the closest hit (id + entry distance). Ray/box tests use
// the standard slab method and prune whole subtrees whose bounds the query misses — exact, no false
// negatives. Header-only, no GPU; rebuild when geometry changes.
namespace maz::game {

class Bvh {
  public:
    struct Item {
        uint32_t id = 0;
        float min[3] = {0, 0, 0};
        float max[3] = {0, 0, 0};
    };

    // Build from center+half-extent, a convenience for point-ish objects.
    static Item fromCenter(uint32_t id, float cx, float cy, float cz, float ex, float ey,
                           float ez) {
        Item it;
        it.id = id;
        it.min[0] = cx - ex;
        it.min[1] = cy - ey;
        it.min[2] = cz - ez;
        it.max[0] = cx + ex;
        it.max[1] = cy + ey;
        it.max[2] = cz + ez;
        return it;
    }

    // (Re)build the hierarchy over `items`. leafSize caps items per leaf. O(n log n) typical.
    void build(const std::vector<Item>& items, int leafSize = 4) {
        m_leafSize = leafSize < 1 ? 1 : leafSize;
        std::vector<Item> work = items;
        m_root = work.empty() ? nullptr : buildNode(work, 0, work.size(), 0);
        m_size = items.size();
    }

    size_t size() const { return m_size; }
    int nodeCount() const { return m_root ? countNodes(*m_root) : 0; }

    // All item ids whose AABB overlaps the query box.
    std::vector<uint32_t> queryBox(const float bmin[3], const float bmax[3]) const {
        std::vector<uint32_t> out;
        if (m_root) {
            queryBoxNode(*m_root, bmin, bmax, out);
        }
        return out;
    }

    // All item ids whose AABB the ray (origin + t*dir, t in [0, tMax]) enters. Order is
    // unspecified.
    std::vector<uint32_t> raycast(const float origin[3], const float dir[3],
                                  float tMax = std::numeric_limits<float>::infinity()) const {
        std::vector<uint32_t> out;
        if (m_root) {
            raycastNode(*m_root, origin, dir, tMax, out);
        }
        return out;
    }

    // The nearest item the ray enters, with its entry distance. Returns false if nothing is hit.
    bool raycastNearest(const float origin[3], const float dir[3], uint32_t& hitId, float& hitT,
                        float tMax = std::numeric_limits<float>::infinity()) const {
        float best = tMax;
        bool found = false;
        if (m_root) {
            nearestNode(*m_root, origin, dir, best, hitId, found);
        }
        if (found) {
            hitT = best;
        }
        return found;
    }

    // ---- geometry helpers (min/max AABBs) ----
    static bool overlap(const float amin[3], const float amax[3], const float bmin[3],
                        const float bmax[3]) {
        return amin[0] <= bmax[0] && amax[0] >= bmin[0] && amin[1] <= bmax[1] &&
               amax[1] >= bmin[1] && amin[2] <= bmax[2] && amax[2] >= bmin[2];
    }
    // Slab ray/AABB: returns true and the entry distance tHit (>=0) if the ray enters [bmin,bmax]
    // within [0, tMax].
    static bool rayAabb(const float o[3], const float d[3], const float bmin[3],
                        const float bmax[3], float tMax, float& tHit) {
        float tmin = 0.0f;
        float tmax = tMax;
        for (int a = 0; a < 3; ++a) {
            if (d[a] > -1e-9f && d[a] < 1e-9f) {
                if (o[a] < bmin[a] || o[a] > bmax[a]) {
                    return false; // parallel and outside the slab
                }
            } else {
                const float inv = 1.0f / d[a];
                float t1 = (bmin[a] - o[a]) * inv;
                float t2 = (bmax[a] - o[a]) * inv;
                if (t1 > t2) {
                    std::swap(t1, t2);
                }
                tmin = t1 > tmin ? t1 : tmin;
                tmax = t2 < tmax ? t2 : tmax;
                if (tmin > tmax) {
                    return false;
                }
            }
        }
        tHit = tmin;
        return true;
    }

  private:
    struct Node {
        float bmin[3];
        float bmax[3];
        std::unique_ptr<Node> left, right;
        std::vector<Item> items; // populated only in leaves
        bool leaf() const { return !left; }
    };

    std::unique_ptr<Node> buildNode(std::vector<Item>& items, size_t begin, size_t end, int depth) {
        auto node = std::make_unique<Node>();
        computeBounds(items, begin, end, node->bmin, node->bmax);
        const size_t count = end - begin;
        if (count <= static_cast<size_t>(m_leafSize) || depth > 64) {
            node->items.assign(items.begin() + static_cast<std::ptrdiff_t>(begin),
                               items.begin() + static_cast<std::ptrdiff_t>(end));
            return node;
        }
        // Split along the axis of greatest centroid extent, at the median centroid.
        const int axis = longestCentroidAxis(items, begin, end);
        const size_t mid = begin + count / 2;
        std::nth_element(items.begin() + static_cast<std::ptrdiff_t>(begin),
                         items.begin() + static_cast<std::ptrdiff_t>(mid),
                         items.begin() + static_cast<std::ptrdiff_t>(end),
                         [axis](const Item& a, const Item& b) {
                             return (a.min[axis] + a.max[axis]) < (b.min[axis] + b.max[axis]);
                         });
        node->left = buildNode(items, begin, mid, depth + 1);
        node->right = buildNode(items, mid, end, depth + 1);
        return node;
    }

    static void computeBounds(const std::vector<Item>& items, size_t begin, size_t end,
                              float bmin[3], float bmax[3]) {
        for (int a = 0; a < 3; ++a) {
            bmin[a] = std::numeric_limits<float>::infinity();
            bmax[a] = -std::numeric_limits<float>::infinity();
        }
        for (size_t i = begin; i < end; ++i) {
            for (int a = 0; a < 3; ++a) {
                bmin[a] = std::min(bmin[a], items[i].min[a]);
                bmax[a] = std::max(bmax[a], items[i].max[a]);
            }
        }
    }
    static int longestCentroidAxis(const std::vector<Item>& items, size_t begin, size_t end) {
        float lo[3] = {1e30f, 1e30f, 1e30f};
        float hi[3] = {-1e30f, -1e30f, -1e30f};
        for (size_t i = begin; i < end; ++i) {
            for (int a = 0; a < 3; ++a) {
                const float c = (items[i].min[a] + items[i].max[a]) * 0.5f;
                lo[a] = std::min(lo[a], c);
                hi[a] = std::max(hi[a], c);
            }
        }
        int axis = 0;
        float best = hi[0] - lo[0];
        for (int a = 1; a < 3; ++a) {
            if (hi[a] - lo[a] > best) {
                best = hi[a] - lo[a];
                axis = a;
            }
        }
        return axis;
    }

    static void queryBoxNode(const Node& n, const float bmin[3], const float bmax[3],
                             std::vector<uint32_t>& out) {
        if (!overlap(n.bmin, n.bmax, bmin, bmax)) {
            return;
        }
        if (n.leaf()) {
            for (const Item& it : n.items) {
                if (overlap(it.min, it.max, bmin, bmax)) {
                    out.push_back(it.id);
                }
            }
            return;
        }
        queryBoxNode(*n.left, bmin, bmax, out);
        queryBoxNode(*n.right, bmin, bmax, out);
    }

    static void raycastNode(const Node& n, const float o[3], const float d[3], float tMax,
                            std::vector<uint32_t>& out) {
        float t;
        if (!rayAabb(o, d, n.bmin, n.bmax, tMax, t)) {
            return;
        }
        if (n.leaf()) {
            for (const Item& it : n.items) {
                float ti;
                if (rayAabb(o, d, it.min, it.max, tMax, ti)) {
                    out.push_back(it.id);
                }
            }
            return;
        }
        raycastNode(*n.left, o, d, tMax, out);
        raycastNode(*n.right, o, d, tMax, out);
    }

    static void nearestNode(const Node& n, const float o[3], const float d[3], float& best,
                            uint32_t& hitId, bool& found) {
        float t;
        if (!rayAabb(o, d, n.bmin, n.bmax, best, t)) {
            return; // node's bounds not reachable within the current best distance
        }
        if (n.leaf()) {
            for (const Item& it : n.items) {
                float ti;
                if (rayAabb(o, d, it.min, it.max, best, ti) && ti < best) {
                    best = ti;
                    hitId = it.id;
                    found = true;
                }
            }
            return;
        }
        nearestNode(*n.left, o, d, best, hitId, found);
        nearestNode(*n.right, o, d, best, hitId, found);
    }

    static int countNodes(const Node& n) {
        if (n.leaf()) {
            return 1;
        }
        return 1 + countNodes(*n.left) + countNodes(*n.right);
    }

    std::unique_ptr<Node> m_root;
    int m_leafSize = 4;
    size_t m_size = 0;
};

} // namespace maz::game
