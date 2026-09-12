#pragma once

#include <cstdint>
#include <unordered_map>

namespace maz::core {

// A generic reference-counted resource cache — the core of an asset manager. Resources are keyed
// (typically by path or a string id); the first acquire() of a key builds the value via a loader
// callback and stores it, and every later acquire() of the same key returns the SAME instance and
// bumps a reference count. release() drops a reference and, when the count reaches zero, evicts the
// entry (optionally running an unload callback to free a GPU/file handle first). This is what lets
// a game request the same texture/mesh/sound a hundred times but load it once. Header-only, no GPU;
// works for any key/value pair, so it unit-tests without a renderer.
//
// std::unordered_map is used for storage: references/pointers to a value stay valid across inserts
// and erases of OTHER keys, so a T& returned by acquire() remains valid until that key is evicted.
template <typename Key, typename T>
class ResourceCache {
public:
    // Acquire the resource for `key`. On the first request `loader()` builds it and it is cached;
    // later requests return the cached instance. Each call increments the key's reference count.
    template <typename Loader>
    T& acquire(const Key& key, Loader&& loader) {
        auto it = m_entries.find(key);
        if (it != m_entries.end()) {
            ++it->second.refs;
            ++m_hits;
            return it->second.value;
        }
        ++m_loads;
        auto res = m_entries.emplace(key, Entry{loader(), 1});
        return res.first->second.value;
    }

    // Release one reference to `key`. When the count reaches zero, `onEvict(value)` runs and the
    // entry is erased. Returns true if this call evicted the entry. Releasing an unknown or already-
    // evicted key is a harmless no-op that returns false.
    template <typename OnEvict>
    bool release(const Key& key, OnEvict&& onEvict) {
        auto it = m_entries.find(key);
        if (it == m_entries.end()) {
            return false;
        }
        if (--it->second.refs == 0) {
            onEvict(it->second.value);
            m_entries.erase(it);
            return true;
        }
        return false;
    }
    bool release(const Key& key) {
        return release(key, [](T&) {});
    }

    bool contains(const Key& key) const { return m_entries.find(key) != m_entries.end(); }
    // Current reference count for `key` (0 if not present).
    uint32_t refCount(const Key& key) const {
        auto it = m_entries.find(key);
        return it == m_entries.end() ? 0u : it->second.refs;
    }
    // Pointer to the cached value, or nullptr if the key isn't loaded. Does not change refcounts.
    T* find(const Key& key) {
        auto it = m_entries.find(key);
        return it == m_entries.end() ? nullptr : &it->second.value;
    }
    const T* find(const Key& key) const {
        auto it = m_entries.find(key);
        return it == m_entries.end() ? nullptr : &it->second.value;
    }

    size_t size() const { return m_entries.size(); } // distinct live resources
    uint64_t loads() const { return m_loads; }       // loader invocations (unique first-loads)
    uint64_t hits() const { return m_hits; }          // acquires that reused a cached resource

    // Evict everything, running `onEvict` on each value first (order unspecified).
    template <typename OnEvict>
    void clear(OnEvict&& onEvict) {
        for (auto& kv : m_entries) {
            onEvict(kv.second.value);
        }
        m_entries.clear();
    }
    void clear() {
        m_entries.clear();
    }

private:
    struct Entry {
        T value;
        uint32_t refs;
    };
    std::unordered_map<Key, Entry> m_entries;
    uint64_t m_loads = 0;
    uint64_t m_hits = 0;
};

} // namespace maz::core
