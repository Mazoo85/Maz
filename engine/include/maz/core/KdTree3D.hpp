#pragma once

#include "maz/math/Math.hpp" // math::vec3

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::core 3D k-d tree — the volumetric companion to KdTree2D: a balanced spatial index for 3D point sets
// that answers nearest-neighbour, k-nearest, and radius queries fast. Maz's other 3D structures (Octree,
// BVH, sweep-and-prune) are tuned for BOXES and ray casts; a k-d tree is the standard choice for POINT
// proximity in 3D — "which N points are nearest this one" drives 3D boids/flocking neighbour lists,
// particle/photon gathering, mesh-vertex welding & dedup, point-cloud registration, and nav-waypoint
// snapping in space. Median-split build (O(n log n), cycling through x/y/z split axes); queries prune by
// splitting-plane distance so they stay ~O(log n) on balanced data. Header-only, deterministic; unit-tested
// against brute force.
namespace maz::core {

class KdTree3D {
  public:
    KdTree3D() = default;
    explicit KdTree3D(const std::vector<math::vec3>& points) { build(points); }

    void build(const std::vector<math::vec3>& points) {
        m_points = points;
        m_nodes.assign(points.size(), Node{});
        m_root = -1;
        std::vector<int> idx(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            idx[i] = static_cast<int>(i);
        }
        m_root = buildRange(idx, 0, static_cast<int>(idx.size()), 0);
    }

    std::size_t size() const { return m_points.size(); }
    const math::vec3& point(int i) const { return m_points[static_cast<std::size_t>(i)]; }

    // Index of the point nearest `q`, or -1 if the tree is empty.
    int nearest(math::vec3 q) const {
        int best = -1;
        float bestD2 = 0.0f;
        nearestRec(m_root, q, 0, best, bestD2);
        return best;
    }

    // Indices of the k nearest points to `q`, sorted nearest-first (fewer if the tree has < k points).
    std::vector<int> kNearest(math::vec3 q, int k) const {
        std::vector<Heap> heap; // max-heap by distance, size <= k
        if (k > 0) {
            kNearestRec(m_root, q, 0, k, heap);
        }
        std::sort(heap.begin(), heap.end(),
                  [](const Heap& a, const Heap& b) { return a.d2 < b.d2; });
        std::vector<int> out;
        out.reserve(heap.size());
        for (const Heap& h : heap) {
            out.push_back(h.idx);
        }
        return out;
    }

    // Indices of all points within `radius` of `q` (unordered).
    std::vector<int> radius(math::vec3 q, float r) const {
        std::vector<int> out;
        radiusRec(m_root, q, r * r, 0, out);
        return out;
    }

  private:
    struct Node {
        int left = -1;
        int right = -1;
        int point = -1; // index into m_points
    };
    struct Heap {
        float d2;
        int idx;
    };

    static float dist2(math::vec3 a, math::vec3 b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }
    static float axisVal(math::vec3 p, int axis) { return axis == 0 ? p.x : (axis == 1 ? p.y : p.z); }

    int buildRange(std::vector<int>& idx, int lo, int hi, int depth) {
        if (lo >= hi) {
            return -1;
        }
        const int axis = depth % 3;
        const int mid = (lo + hi) / 2;
        std::nth_element(idx.begin() + lo, idx.begin() + mid, idx.begin() + hi,
                         [&](int a, int b) {
                             return axisVal(m_points[static_cast<std::size_t>(a)], axis) <
                                    axisVal(m_points[static_cast<std::size_t>(b)], axis);
                         });
        const int nodeIdx = idx[static_cast<std::size_t>(mid)];
        Node& n = m_nodes[static_cast<std::size_t>(nodeIdx)];
        n.point = nodeIdx;
        n.left = buildRange(idx, lo, mid, depth + 1);
        n.right = buildRange(idx, mid + 1, hi, depth + 1);
        return nodeIdx;
    }

    void nearestRec(int node, math::vec3 q, int depth, int& best, float& bestD2) const {
        if (node < 0) {
            return;
        }
        const Node& n = m_nodes[static_cast<std::size_t>(node)];
        const math::vec3 p = m_points[static_cast<std::size_t>(n.point)];
        const float d2 = dist2(p, q);
        if (best < 0 || d2 < bestD2) {
            best = n.point;
            bestD2 = d2;
        }
        const int axis = depth % 3;
        const float diff = axisVal(q, axis) - axisVal(p, axis);
        const int near = diff < 0 ? n.left : n.right;
        const int far = diff < 0 ? n.right : n.left;
        nearestRec(near, q, depth + 1, best, bestD2);
        if (diff * diff < bestD2) {
            nearestRec(far, q, depth + 1, best, bestD2);
        }
    }

    void kNearestRec(int node, math::vec3 q, int depth, int k, std::vector<Heap>& heap) const {
        if (node < 0) {
            return;
        }
        const Node& n = m_nodes[static_cast<std::size_t>(node)];
        const math::vec3 p = m_points[static_cast<std::size_t>(n.point)];
        const float d2 = dist2(p, q);
        if (static_cast<int>(heap.size()) < k) {
            heap.push_back({d2, n.point});
            std::push_heap(heap.begin(), heap.end(),
                           [](const Heap& a, const Heap& b) { return a.d2 < b.d2; });
        } else if (d2 < heap.front().d2) {
            std::pop_heap(heap.begin(), heap.end(),
                          [](const Heap& a, const Heap& b) { return a.d2 < b.d2; });
            heap.back() = {d2, n.point};
            std::push_heap(heap.begin(), heap.end(),
                           [](const Heap& a, const Heap& b) { return a.d2 < b.d2; });
        }
        const int axis = depth % 3;
        const float diff = axisVal(q, axis) - axisVal(p, axis);
        const int near = diff < 0 ? n.left : n.right;
        const int far = diff < 0 ? n.right : n.left;
        kNearestRec(near, q, depth + 1, k, heap);
        const float worst = (static_cast<int>(heap.size()) < k) ? 3.4e38f : heap.front().d2;
        if (diff * diff < worst) {
            kNearestRec(far, q, depth + 1, k, heap);
        }
    }

    void radiusRec(int node, math::vec3 q, float r2, int depth, std::vector<int>& out) const {
        if (node < 0) {
            return;
        }
        const Node& n = m_nodes[static_cast<std::size_t>(node)];
        const math::vec3 p = m_points[static_cast<std::size_t>(n.point)];
        if (dist2(p, q) <= r2) {
            out.push_back(n.point);
        }
        const int axis = depth % 3;
        const float diff = axisVal(q, axis) - axisVal(p, axis);
        const int near = diff < 0 ? n.left : n.right;
        const int far = diff < 0 ? n.right : n.left;
        radiusRec(near, q, r2, depth + 1, out);
        if (diff * diff <= r2) {
            radiusRec(far, q, r2, depth + 1, out);
        }
    }

    std::vector<math::vec3> m_points;
    std::vector<Node> m_nodes;
    int m_root = -1;
};

} // namespace maz::core
