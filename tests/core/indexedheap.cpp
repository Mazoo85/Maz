// tests/core/indexedheap.cpp — verifies the indexed binary min-heap (IndexedHeap.hpp).
// Ground truths, deterministic:
//   * popping a heap of many pushed priorities yields them in non-decreasing order (heapsort);
//   * top() always exposes the current minimum;
//   * decreaseKey moves an element earlier — it pops before elements it was originally behind;
//   * update to a WORSE priority sinks an element so it pops later;
//   * push on an existing key updates rather than duplicates;
//   * contains / priorityOf / erase behave; a Dijkstra-style relaxation loop finds correct distances.
#include "maz/core/IndexedHeap.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::IndexedHeap;

int main() {
    // --- 1. Heapsort: pops come out sorted. ---
    {
        IndexedHeap<int, int> h;
        const int vals[] = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
        for (int i = 0; i < 10; ++i) h.push(i, vals[i]);
        CHECK(h.size() == 10, "size reflects pushes");
        int prev = -1;
        bool sorted = true;
        while (!h.empty()) {
            const int k = h.pop();
            if (vals[k] < prev) sorted = false;
            prev = vals[k];
        }
        CHECK(sorted, "pops emerge in non-decreasing priority order");
        CHECK(h.empty(), "heap empties out");
    }

    // --- 2. top() exposes the current minimum. ---
    {
        IndexedHeap<int, int> h;
        h.push(10, 50);
        h.push(20, 20);
        h.push(30, 80);
        CHECK(h.top() == 20 && h.topPriority() == 20, "top is the minimum-priority key");
        h.push(40, 5);
        CHECK(h.top() == 40, "a smaller push becomes the new top");
    }

    // --- 3. decreaseKey reorders the pops. ---
    {
        IndexedHeap<int, int> h;
        h.push(1, 100);
        h.push(2, 200);
        h.push(3, 300);
        // Key 3 was worst; drop it below everything.
        const bool changed = h.decreaseKey(3, 10);
        CHECK(changed, "decreaseKey applies when the new priority is better");
        CHECK(h.pop() == 3, "the decreased key now pops first");
        CHECK(!h.decreaseKey(1, 500), "decreaseKey is a no-op when the new priority is worse");
    }

    // --- 4. update to a worse priority sinks the element. ---
    {
        IndexedHeap<int, int> h;
        h.push(1, 10);
        h.push(2, 20);
        h.push(3, 30);
        h.update(1, 100); // was best, now worst
        CHECK(h.pop() == 2, "after worsening key 1, key 2 pops first");
        CHECK(h.pop() == 3, "then key 3");
        CHECK(h.pop() == 1, "and the worsened key 1 last");
    }

    // --- 5. push on existing key updates, not duplicates. ---
    {
        IndexedHeap<int, int> h;
        h.push(7, 50);
        h.push(7, 5); // same key, new priority
        CHECK(h.size() == 1, "re-pushing a key does not duplicate it");
        CHECK(h.priorityOf(7) == 5, "re-push updates the priority");
    }

    // --- 6. contains / priorityOf / erase. ---
    {
        IndexedHeap<int, int> h;
        h.push(1, 10);
        h.push(2, 20);
        CHECK(h.contains(1) && !h.contains(99), "contains reports membership");
        CHECK(h.priorityOf(2) == 20, "priorityOf returns the stored priority");
        CHECK(h.erase(1) && !h.contains(1), "erase removes a key");
        CHECK(!h.erase(1), "erasing an absent key returns false");
        CHECK(h.top() == 2, "the heap stays valid after erase");
    }

    // --- 7. Dijkstra relaxation on a tiny graph gives correct distances. ---
    {
        // Graph: 0->1 (4), 0->2 (1), 2->1 (2), 1->3 (1), 2->3 (5). Shortest 0->3 = 0-2-1-3 = 1+2+1 = 4.
        struct Edge { int to; int w; };
        std::vector<std::vector<Edge>> adj(4);
        adj[0] = {{1, 4}, {2, 1}};
        adj[2] = {{1, 2}, {3, 5}};
        adj[1] = {{3, 1}};
        std::vector<int> dist(4, 1 << 30);
        IndexedHeap<int, int> pq;
        dist[0] = 0;
        pq.push(0, 0);
        while (!pq.empty()) {
            const int u = pq.pop();
            for (const Edge& e : adj[static_cast<size_t>(u)]) {
                const int nd = dist[static_cast<size_t>(u)] + e.w;
                if (nd < dist[static_cast<size_t>(e.to)]) {
                    dist[static_cast<size_t>(e.to)] = nd;
                    pq.decreaseKey(e.to, nd); // relax: insert or improve
                }
            }
        }
        CHECK(dist[3] == 4, "Dijkstra via the heap finds the shortest path 0->2->1->3 = 4");
        CHECK(dist[2] == 1 && dist[1] == 3, "intermediate distances are correct");
    }

    if (g_fail == 0) {
        std::printf("indexedheap: OK — heapsort, top, decreaseKey reorder, worsen-sink, update-not-"
                    "duplicate, contains/erase, Dijkstra.\n");
        return 0;
    }
    std::printf("indexedheap: %d failure(s).\n", g_fail);
    return 1;
}
