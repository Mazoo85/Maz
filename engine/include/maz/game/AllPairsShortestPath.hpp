#pragma once

#include <cstddef>
#include <limits>
#include <vector>

// maz::game all-pairs shortest paths (Floyd-Warshall) — the shortest distance between EVERY pair of
// nodes in a weighted graph, precomputed in one O(V^3) pass. Unlike the engine's single-source
// pathfinders (AStar2D, DijkstraMap), this fills a whole distance matrix at once, which is the right
// tool for a precomputed routing/influence table on a small graph: "distance from every room to every
// other room", "which of my N bases is nearest to each threat", static AI cost tables. It handles
// negative edge weights (as long as there is no negative cycle, which it detects) and reconstructs the
// actual path via a next-hop matrix. Godot's AStar is single-pair, so all-pairs is a beyond-Godot
// utility. Header-only, std-only, deterministic.
namespace maz::game {

struct WEdge {
    int from = 0;
    int to = 0;
    float weight = 1.0f;
    bool bidirectional = true; // true = undirected edge (both directions); false = directed from->to
};

struct AllPairsResult {
    int n = 0;
    std::vector<float> dist; // n*n row-major; +infinity where unreachable
    std::vector<int> next;   // n*n next hop from i toward j (-1 if no path)
    bool hasNegativeCycle = false;

    static float inf() { return std::numeric_limits<float>::infinity(); }

    // Shortest distance from i to j (+infinity if unreachable).
    float at(int i, int j) const {
        return dist[static_cast<std::size_t>(i) * static_cast<std::size_t>(n)
                    + static_cast<std::size_t>(j)];
    }

    // Reconstructed node sequence from i to j (inclusive). Empty if there is no path.
    std::vector<int> path(int i, int j) const {
        if (i < 0 || j < 0 || i >= n || j >= n) {
            return {};
        }
        const std::size_t N = static_cast<std::size_t>(n);
        if (next[static_cast<std::size_t>(i) * N + static_cast<std::size_t>(j)] < 0) {
            return {};
        }
        std::vector<int> p{i};
        while (i != j) {
            i = next[static_cast<std::size_t>(i) * N + static_cast<std::size_t>(j)];
            if (i < 0) {
                return {};
            }
            p.push_back(i);
        }
        return p;
    }
};

// Compute all-pairs shortest paths over `nodeCount` nodes in [0, nodeCount). Out-of-range or self-loop
// edges are ignored; parallel edges keep the lightest. If a negative cycle exists, `hasNegativeCycle`
// is set (the distance matrix is then only meaningful for pairs not routed through the cycle).
inline AllPairsResult allPairsShortestPaths(int nodeCount, const std::vector<WEdge>& edges) {
    AllPairsResult r;
    if (nodeCount <= 0) {
        return r;
    }
    r.n = nodeCount;
    const std::size_t N = static_cast<std::size_t>(nodeCount);
    r.dist.assign(N * N, AllPairsResult::inf());
    r.next.assign(N * N, -1);
    for (std::size_t i = 0; i < N; ++i) {
        r.dist[i * N + i] = 0.0f;
        r.next[i * N + i] = static_cast<int>(i);
    }
    auto relaxEdge = [&](int a, int b, float w) {
        const std::size_t idx = static_cast<std::size_t>(a) * N + static_cast<std::size_t>(b);
        if (w < r.dist[idx]) {
            r.dist[idx] = w;
            r.next[idx] = b;
        }
    };
    for (const WEdge& e : edges) {
        if (e.from < 0 || e.to < 0 || e.from >= nodeCount || e.to >= nodeCount || e.from == e.to) {
            continue;
        }
        relaxEdge(e.from, e.to, e.weight);
        if (e.bidirectional) {
            relaxEdge(e.to, e.from, e.weight);
        }
    }

    for (std::size_t k = 0; k < N; ++k) {
        for (std::size_t i = 0; i < N; ++i) {
            const float dik = r.dist[i * N + k];
            if (dik == AllPairsResult::inf()) {
                continue;
            }
            for (std::size_t j = 0; j < N; ++j) {
                const float through = dik + r.dist[k * N + j];
                if (through < r.dist[i * N + j]) {
                    r.dist[i * N + j] = through;
                    r.next[i * N + j] = r.next[i * N + k];
                }
            }
        }
    }
    for (std::size_t i = 0; i < N; ++i) {
        if (r.dist[i * N + i] < 0.0f) {
            r.hasNegativeCycle = true;
            break;
        }
    }
    return r;
}

} // namespace maz::game
