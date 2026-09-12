#pragma once

#include <cstddef>
#include <deque>
#include <vector>

// maz::core::ObjectPool<T> — a typed recycling pool that hands out reusable objects and takes them back, so a
// game can spawn and despawn bullets, particles, enemies, damage numbers, or temporary buffers every frame
// WITHOUT churning the allocator. acquire() reuses a previously-released slot if one is free, otherwise grows
// by one; release() returns a slot to the free list (without destroying it) so the next acquire() reuses it.
// Capacity therefore rises only to the high-water mark of simultaneously-live objects and then stops — the
// whole point of pooling. Distinct from the engine's other two facilities: PoolAllocator hands out raw memory
// bytes, and SlotMap is a generational handle→value map with stable IDs across reuse; this is the simple,
// index-addressed live-object recycler most gameplay code actually reaches for. Backed by a std::deque so a
// reference returned by get() stays valid even as the pool grows. Recycled objects keep their previous value
// (the caller re-initialises on acquire) — pooling reuses storage, it does not reset it. Header-only, std-only.
namespace maz::core {

template <typename T>
class ObjectPool {
public:
    // Acquire a slot: reuse a released one if available, otherwise append a fresh default-constructed T.
    // Returns the slot's index (a stable handle for get()/release()).
    std::size_t acquire() {
        if (!m_free.empty()) {
            const std::size_t i = m_free.back();
            m_free.pop_back();
            m_active[i] = 1;
            ++m_activeCount;
            return i;
        }
        const std::size_t i = m_storage.size();
        m_storage.emplace_back();
        m_active.push_back(1);
        ++m_activeCount;
        return i;
    }

    T& get(std::size_t index) { return m_storage[index]; }
    const T& get(std::size_t index) const { return m_storage[index]; }

    // Return a slot to the pool for reuse. Ignores an out-of-range or already-free index (no double-free).
    void release(std::size_t index) {
        if (index >= m_active.size() || m_active[index] == 0) return;
        m_active[index] = 0;
        m_free.push_back(index);
        --m_activeCount;
    }

    bool isActive(std::size_t index) const { return index < m_active.size() && m_active[index] != 0; }

    std::size_t activeCount() const { return m_activeCount; }   // live objects right now
    std::size_t capacity() const { return m_storage.size(); }   // total slots ever created (the high-water mark)

    // Release every live slot back to the pool, keeping the allocated capacity for reuse.
    void reset() {
        m_free.clear();
        m_free.reserve(m_active.size());
        for (std::size_t i = 0; i < m_active.size(); ++i) {
            m_active[i] = 0;
            m_free.push_back(i);
        }
        m_activeCount = 0;
    }

    // Drop everything, including the backing storage (capacity returns to 0).
    void clear() {
        m_storage.clear();
        m_active.clear();
        m_free.clear();
        m_activeCount = 0;
    }

private:
    std::deque<T> m_storage;         // stable element addresses as it grows
    std::vector<char> m_active;      // per-slot live flag
    std::vector<std::size_t> m_free; // indices available for reuse
    std::size_t m_activeCount = 0;
};

} // namespace maz::core
