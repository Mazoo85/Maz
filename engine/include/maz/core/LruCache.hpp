#pragma once

#include <list>
#include <unordered_map>
#include <cstddef>
#include <utility>  // std::pair, std::move

#include "maz/core/Assert.hpp"

namespace maz::core {

// A fixed-capacity least-recently-used cache — put inserts or updates a key
// (promoting it to most-recently-used and evicting the least-recently-used
// entry when over capacity); get returns a pointer to the value AND promotes
// it to MRU (nullptr on miss); peek reads without changing recency;
// contains/erase/size/clear round it out. Backed by an intrusive MRU list + a
// key->node map (list iterators stay valid across other mutations). The Godot
// resource-cache analog. Pointers returned by get/peek are invalidated by the
// next put/erase/clear that removes that entry. NOT thread-safe.
//
// A TTL / write-back variant is a future refinement.
template <typename K, typename V, typename Hash = std::hash<K>>
class LruCache {
public:
    explicit LruCache(std::size_t capacity) : m_capacity(capacity) {
        MAZ_ASSERT(capacity >= 1, "LruCache: capacity must be >= 1");
        m_map.reserve(capacity);
    }

    // Insert a new key at MRU, or update an existing key's value and promote it
    // to MRU. When an insert pushes size past capacity, the LRU (back) entry is
    // evicted AFTER inserting, so a put that both inserts and overflows keeps
    // the new entry and drops the oldest.
    void put(const K& key, V value) {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            it->second->second = std::move(value);
            m_order.splice(m_order.begin(), m_order, it->second);
            return;
        }
        m_order.emplace_front(key, std::move(value));
        m_map[key] = m_order.begin();
        if (m_map.size() > m_capacity) {
            m_map.erase(m_order.back().first);
            m_order.pop_back();
        }
    }

    // Lookup that PROMOTES the hit to MRU; returns a pointer to the stored value
    // (valid until the next mutation that could evict it) or nullptr on miss.
    V* get(const K& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return nullptr;
        }
        m_order.splice(m_order.begin(), m_order, it->second);
        return &it->second->second;
    }

    // NON-promoting read — does NOT change recency (useful for inspection).
    const V* peek(const K& key) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? nullptr : &it->second->second;
    }

    // True if key is present; does NOT promote.
    bool contains(const K& key) const { return m_map.find(key) != m_map.end(); }

    // Remove a key; false if it was absent.
    bool erase(const K& key) {
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return false;
        }
        m_order.erase(it->second);
        m_map.erase(it);
        return true;
    }

    std::size_t size() const { return m_map.size(); }
    std::size_t capacity() const { return m_capacity; }
    bool empty() const { return m_map.empty(); }

    void clear() {
        m_order.clear();
        m_map.clear();
    }

private:
    using Entry = std::pair<K, V>;

    std::list<Entry> m_order;  // FRONT = most-recently-used, BACK = least-recently-used
    std::unordered_map<K, typename std::list<Entry>::iterator, Hash> m_map;
    std::size_t m_capacity;
};

} // namespace maz::core
