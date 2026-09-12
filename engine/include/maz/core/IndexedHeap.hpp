#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

// maz::core indexed binary min-heap with decrease-key — a priority queue whose entries can be UPDATED.
//
// A plain std::priority_queue can push and pop by priority but cannot change the priority of an element
// already inside it — the operation Dijkstra, A*, and event/timer queues need constantly ("this node's
// tentative cost just dropped; re-prioritise it"). Without it you push duplicates and filter stale pops;
// with it the queue stays tight. This is the classic binary heap paired with a key->slot index map, so
// push / pop / decreaseKey / update / contains / erase are all O(log n) (contains and priorityOf are
// O(1)). Keyed by a caller id (e.g. a node index), min-heap by default (smallest priority first) with a
// custom comparator allowed. Header-only, deterministic — unit-tested as a heapsort, and for decrease-key
// actually reordering the pops.
namespace maz::core {

template <class Key, class Priority, class Compare = std::less<Priority>>
class IndexedHeap {
public:
    IndexedHeap() = default;
    explicit IndexedHeap(Compare cmp) : cmp_(cmp) {}

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
    void clear() {
        heap_.clear();
        slot_.clear();
    }

    bool contains(const Key& k) const { return slot_.find(k) != slot_.end(); }

    // Priority currently associated with `k` (undefined if absent — check contains() first).
    Priority priorityOf(const Key& k) const { return heap_[slot_.at(k)].second; }

    // Insert `k` with `priority`. If `k` is already present, its priority is updated (up or down) and it
    // is re-positioned — so push doubles as a general "set priority".
    void push(const Key& k, const Priority& priority) {
        const auto it = slot_.find(k);
        if (it != slot_.end()) {
            update(k, priority);
            return;
        }
        heap_.emplace_back(k, priority);
        const std::size_t i = heap_.size() - 1;
        slot_[k] = i;
        siftUp(i);
    }

    // The key with the best (min) priority. Precondition: !empty().
    const Key& top() const { return heap_.front().first; }
    const Priority& topPriority() const { return heap_.front().second; }

    // Remove and return the best (min-priority) key. Precondition: !empty().
    Key pop() {
        const Key best = heap_.front().first;
        removeAt(0);
        return best;
    }

    // Change the priority of an existing key and restore heap order. No-op semantics if absent = false.
    bool update(const Key& k, const Priority& priority) {
        const auto it = slot_.find(k);
        if (it == slot_.end()) return false;
        const std::size_t i = it->second;
        const Priority old = heap_[i].second;
        heap_[i].second = priority;
        if (cmp_(priority, old)) {
            siftUp(i); // improved (smaller) -> may rise
        } else if (cmp_(old, priority)) {
            siftDown(i); // worsened (larger) -> may sink
        }
        return true;
    }

    // Convenience: only apply if the new priority is strictly better (smaller) than the current one.
    // Returns true if it changed anything. This is the exact Dijkstra/A* relaxation step.
    bool decreaseKey(const Key& k, const Priority& priority) {
        const auto it = slot_.find(k);
        if (it == slot_.end()) {
            push(k, priority);
            return true;
        }
        if (cmp_(priority, heap_[it->second].second)) {
            heap_[it->second].second = priority;
            siftUp(it->second);
            return true;
        }
        return false;
    }

    // Remove an arbitrary key. Returns false if it wasn't present.
    bool erase(const Key& k) {
        const auto it = slot_.find(k);
        if (it == slot_.end()) return false;
        removeAt(it->second);
        return true;
    }

private:
    void removeAt(std::size_t i) {
        const std::size_t last = heap_.size() - 1;
        slot_.erase(heap_[i].first);
        if (i != last) {
            heap_[i] = heap_[last];
            slot_[heap_[i].first] = i;
        }
        heap_.pop_back();
        if (i < heap_.size()) {
            // Restore in whichever direction is needed.
            siftUp(i);
            siftDown(i);
        }
    }

    void siftUp(std::size_t i) {
        while (i > 0) {
            const std::size_t parent = (i - 1) / 2;
            if (cmp_(heap_[i].second, heap_[parent].second)) {
                swapNodes(i, parent);
                i = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(std::size_t i) {
        const std::size_t n = heap_.size();
        while (true) {
            const std::size_t l = 2 * i + 1;
            const std::size_t r = 2 * i + 2;
            std::size_t best = i;
            if (l < n && cmp_(heap_[l].second, heap_[best].second)) best = l;
            if (r < n && cmp_(heap_[r].second, heap_[best].second)) best = r;
            if (best == i) break;
            swapNodes(i, best);
            i = best;
        }
    }

    void swapNodes(std::size_t a, std::size_t b) {
        std::swap(heap_[a], heap_[b]);
        slot_[heap_[a].first] = a;
        slot_[heap_[b].first] = b;
    }

    std::vector<std::pair<Key, Priority>> heap_;
    std::unordered_map<Key, std::size_t> slot_;
    Compare cmp_{};
};

} // namespace maz::core
