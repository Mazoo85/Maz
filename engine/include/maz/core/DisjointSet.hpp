#pragma once

#include <cstddef>
#include <utility>
#include <vector>

// maz::core::DisjointSet — union-find over [0, n): the classic near-O(1) structure for tracking which
// elements belong to the same group as pairs get merged. Path compression plus union-by-rank keep
// find()/unite() effectively constant time (inverse-Ackermann amortised). The go-to tool for
// connected-components queries, Kruskal minimum-spanning-tree, maze generation (carve a wall only when
// it joins two different regions), flood-region merging in a tile map, island/cluster counting, and
// "are these two things reachable" checks — all things a game needs and Godot ships no primitive for.
// It also maintains a live count of disjoint sets and each set's size. Header-only, std-only.
namespace maz::core {

class DisjointSet {
public:
    DisjointSet() = default;
    explicit DisjointSet(std::size_t n) { reset(n); }

    // (Re)initialise to n singleton sets {0}, {1}, ... {n-1}.
    void reset(std::size_t n) {
        m_parent.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            m_parent[i] = i;
        }
        m_rank.assign(n, 0);
        m_size.assign(n, 1);
        m_count = n;
    }

    std::size_t size() const { return m_parent.size(); } // number of elements
    std::size_t count() const { return m_count; }        // number of disjoint sets

    // Representative (root) of x's set, with path halving so repeat calls get cheaper.
    std::size_t find(std::size_t x) {
        while (m_parent[x] != x) {
            m_parent[x] = m_parent[m_parent[x]]; // point x at its grandparent (halving)
            x = m_parent[x];
        }
        return x;
    }

    bool connected(std::size_t a, std::size_t b) { return find(a) == find(b); }

    // Merge the sets containing a and b. Returns true if they were distinct (a merge happened),
    // false if they were already in the same set.
    bool unite(std::size_t a, std::size_t b) {
        std::size_t ra = find(a);
        std::size_t rb = find(b);
        if (ra == rb) {
            return false;
        }
        // Attach the shorter tree under the taller (union by rank).
        if (m_rank[ra] < m_rank[rb]) {
            std::swap(ra, rb);
        }
        m_parent[rb] = ra;
        m_size[ra] += m_size[rb];
        if (m_rank[ra] == m_rank[rb]) {
            ++m_rank[ra];
        }
        --m_count;
        return true;
    }

    // Size of the set that contains x.
    std::size_t componentSize(std::size_t x) { return m_size[find(x)]; }

private:
    std::vector<std::size_t> m_parent;
    std::vector<std::size_t> m_rank; // upper bound on tree height, for balanced unions
    std::vector<std::size_t> m_size; // size of the set rooted here (valid at roots)
    std::size_t m_count = 0;
};

} // namespace maz::core
