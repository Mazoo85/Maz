#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::core::IntervalTree — answer "which intervals contain point x?" and "which intervals overlap
// [a,b]?" in O(log n + k) instead of scanning all n intervals every query. An interval is a [low, high]
// range carrying a payload. This is the right structure whenever many time-ranges or 1-D spans must be
// queried repeatedly: which animation clips / audio cues / cutscene triggers are ACTIVE at the current
// playhead time; which reservations overlap a requested window; which spans on one axis touch a probe (a
// 1-D broadphase). The naive approach re-tests every interval per query — fine for a handful, wasteful for
// hundreds queried each frame. This is a STATIC augmented interval tree: insert all intervals, build()
// once (a height-balanced BST ordered by low endpoint, each node augmented with the maximum high endpoint
// in its subtree), then query many times; the max-endpoint augmentation prunes whole subtrees that cannot
// reach the query. Header-only, std-only, deterministic. Godot has no interval tree.
namespace maz::core {

template <class T>
class IntervalTree {
public:
    // Register an interval [low, high] (inclusive) with a payload. Call before build().
    void insert(double low, double high, const T& value) {
        if (high < low) {
            std::swap(low, high);
        }
        m_items.push_back(Item{low, high, value});
        m_built = false;
    }

    // Build the tree. Call once after all inserts, before querying. Idempotent.
    void build() {
        const std::size_t n = m_items.size();
        m_nodes.clear();
        m_nodes.reserve(n);
        // Order items by low endpoint; a balanced BST is built by repeatedly taking the median.
        std::vector<std::size_t> order(n);
        for (std::size_t i = 0; i < n; ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
            if (m_items[a].low != m_items[b].low) {
                return m_items[a].low < m_items[b].low;
            }
            return m_items[a].high < m_items[b].high;
        });
        m_root = buildRec(order, 0, static_cast<std::ptrdiff_t>(n) - 1);
        m_built = true;
    }

    // Payloads of every interval that contains x (low <= x <= high).
    std::vector<T> queryPoint(double x) const { return queryOverlap(x, x); }

    // Payloads of every interval that overlaps [a, b] (low <= b && high >= a).
    std::vector<T> queryOverlap(double a, double b) const {
        std::vector<T> out;
        if (b < a) {
            std::swap(a, b);
        }
        if (m_built && m_root != -1) {
            queryRec(m_root, a, b, out);
        }
        return out;
    }

    std::size_t size() const { return m_items.size(); }
    bool empty() const { return m_items.empty(); }
    void clear() {
        m_items.clear();
        m_nodes.clear();
        m_root = -1;
        m_built = false;
    }

private:
    struct Item {
        double low, high;
        T value;
    };
    struct Node {
        std::size_t item;   // index into m_items
        std::ptrdiff_t left = -1;
        std::ptrdiff_t right = -1;
        double maxHigh = 0.0; // max high endpoint over this subtree
    };

    // Build a balanced subtree from order[lo..hi]; returns node index (or -1).
    std::ptrdiff_t buildRec(const std::vector<std::size_t>& order, std::ptrdiff_t lo, std::ptrdiff_t hi) {
        if (lo > hi) {
            return -1;
        }
        const std::ptrdiff_t mid = lo + (hi - lo) / 2;
        const std::ptrdiff_t idx = static_cast<std::ptrdiff_t>(m_nodes.size());
        m_nodes.push_back(Node{order[static_cast<std::size_t>(mid)], -1, -1, 0.0});
        const std::ptrdiff_t l = buildRec(order, lo, mid - 1);
        const std::ptrdiff_t r = buildRec(order, mid + 1, hi);
        Node& node = m_nodes[static_cast<std::size_t>(idx)];
        node.left = l;
        node.right = r;
        double mx = m_items[node.item].high;
        if (l != -1) {
            mx = std::max(mx, m_nodes[static_cast<std::size_t>(l)].maxHigh);
        }
        if (r != -1) {
            mx = std::max(mx, m_nodes[static_cast<std::size_t>(r)].maxHigh);
        }
        node.maxHigh = mx;
        return idx;
    }

    void queryRec(std::ptrdiff_t nodeIdx, double a, double b, std::vector<T>& out) const {
        const Node& node = m_nodes[static_cast<std::size_t>(nodeIdx)];
        // If the largest high endpoint in this subtree is below a, nothing here can overlap.
        if (node.maxHigh < a) {
            return;
        }
        if (node.left != -1) {
            queryRec(node.left, a, b, out);
        }
        const Item& it = m_items[node.item];
        if (it.low <= b && it.high >= a) {
            out.push_back(it.value);
        }
        // Right subtree holds only larger low endpoints; if this node's low already exceeds b, skip it.
        if (it.low <= b && node.right != -1) {
            queryRec(node.right, a, b, out);
        }
    }

    std::vector<Item> m_items;
    std::vector<Node> m_nodes;
    std::ptrdiff_t m_root = -1;
    bool m_built = false;
};

} // namespace maz::core
