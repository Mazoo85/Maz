#pragma once

#include <cstddef>
#include <iterator>
#include <list>
#include <unordered_map>
#include <utility>

// maz::core::LruCache — a fixed-capacity least-recently-used cache. Stores up to `capacity` key->value
// entries; when a new key would overflow, the entry that has gone longest without being touched is
// evicted. Every get()/put() marks its key most-recently-used, so the hot working set survives and the
// cold tail is dropped. The standard bounded-memory memoization tool: caching decoded tiles/chunks,
// pathfinding results, procedural-generation outputs, or any expensive keyed computation you want to
// reuse without unbounded growth. Distinct from ResourceCache (ref-counted asset lifetimes, no
// eviction) — this is a pure size-capped recency cache. O(1) get/put via a hash map into an intrusive
// recency list. Tracks hit/miss counts for tuning. Header-only, std-only. Godot has no generic LRU.
namespace maz::core {

template <typename K, typename V>
class LruCache {
public:
    explicit LruCache(std::size_t capacity) : m_capacity(capacity == 0 ? 1 : capacity) {}

    std::size_t capacity() const { return m_capacity; }
    std::size_t size() const { return m_map.size(); }
    bool empty() const { return m_map.empty(); }
    bool contains(const K& key) const { return m_map.find(key) != m_map.end(); }

    // Look up; on hit, mark most-recently-used and return a pointer to the value, else nullptr.
    // Updates hit/miss counters.
    V* get(const K& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            ++m_misses;
            return nullptr;
        }
        ++m_hits;
        m_items.splice(m_items.begin(), m_items, it->second); // move node to the front (MRU)
        return &it->second->second;
    }

    // Look up without affecting recency or hit/miss counters.
    const V* peek(const K& key) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? nullptr : &it->second->second;
    }

    // Insert or overwrite; marks most-recently-used and evicts the LRU entry if over capacity.
    void put(const K& key, const V& value) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            it->second->second = value;
            m_items.splice(m_items.begin(), m_items, it->second);
            return;
        }
        m_items.emplace_front(key, value);
        m_map[key] = m_items.begin();
        if (m_map.size() > m_capacity) {
            evictLru();
        }
    }

    bool erase(const K& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return false;
        }
        m_items.erase(it->second);
        m_map.erase(it);
        return true;
    }

    void clear() {
        m_items.clear();
        m_map.clear();
    }

    // Most / least recently used key. Only valid when the cache is non-empty.
    const K& mostRecentKey() const { return m_items.front().first; }
    const K& leastRecentKey() const { return m_items.back().first; }

    std::size_t hits() const { return m_hits; }
    std::size_t misses() const { return m_misses; }
    void resetStats() {
        m_hits = 0;
        m_misses = 0;
    }

private:
    using Item = std::pair<K, V>;
    using ListIt = typename std::list<Item>::iterator;

    void evictLru() {
        const ListIt last = std::prev(m_items.end());
        m_map.erase(last->first);
        m_items.pop_back();
    }

    std::size_t m_capacity;
    std::list<Item> m_items;             // front = most-recently-used, back = least-recently-used
    std::unordered_map<K, ListIt> m_map; // key -> node in m_items
    std::size_t m_hits = 0;
    std::size_t m_misses = 0;
};

} // namespace maz::core
