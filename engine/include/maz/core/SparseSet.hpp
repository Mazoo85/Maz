#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>  // std::move

#include "maz/core/Assert.hpp"

namespace maz::core {

// SparseSet<T> is a sparse-set-backed MAP from uint32_t keys to T values —
// O(1) insert/remove/contains/get with PACKED dense storage for cache-friendly
// iteration (the standard ECS component-storage structure). Invariant:
// m_dense.size() == m_values.size() == size() (the live count); m_sparse.size()
// is maxKeyEverInserted+1 (it grows only, never shrinks on remove). T is stored
// by value and assumed default-constructible (like Pool<T>/RingBuffer<T>).
// NOT thread-safe.
template <class T>
class SparseSet {
public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu;  // UINT32_MAX: sparse "absent" sentinel AND reserved (disallowed) key

    // True iff key currently maps to a value. The back-check m_dense[d] == key
    // (a round-trip key -> d -> key) makes contains robust to ANY stale sparse
    // entry, not just kInvalid ones. Cast d to size_t before comparing against
    // .size() (the widths differ; keeps -Wconversion clean).
    bool contains(uint32_t key) const {
        if (key >= static_cast<uint32_t>(m_sparse.size())) {
            return false;
        }
        uint32_t d = m_sparse[key];
        return static_cast<std::size_t>(d) < m_dense.size() && m_dense[d] == key;
    }

    // Insert or upsert. Returns true when a new key/dense slot is created; false
    // when the key was already present and its value was overwritten in place
    // (the ECS upsert idiom, mirroring Pool::destroy/RingBuffer::push where the
    // bool means "did the state change happen").
    bool insert(uint32_t key, const T& value) {
        MAZ_ASSERT(key != kInvalid, "SparseSet: key UINT32_MAX is reserved as the sentinel");
        if (contains(key)) {
            m_values[m_sparse[key]] = value;  // OVERWRITE existing, "not newly inserted"
            return false;
        }
        if (key >= static_cast<uint32_t>(m_sparse.size())) {
            // Widen key to size_t BEFORE the +1 to avoid uint32 overflow; new slots = kInvalid.
            m_sparse.resize(static_cast<std::size_t>(key) + 1u, kInvalid);
        }
        uint32_t newDense = static_cast<uint32_t>(m_dense.size());
        m_sparse[key] = newDense;
        m_dense.push_back(key);
        m_values.push_back(value);
        return true;
    }

    // Move overload; same bool semantics as the copy version.
    bool insert(uint32_t key, T&& value) {
        MAZ_ASSERT(key != kInvalid, "SparseSet: key UINT32_MAX is reserved as the sentinel");
        if (contains(key)) {
            m_values[m_sparse[key]] = std::move(value);  // OVERWRITE existing, "not newly inserted"
            return false;
        }
        if (key >= static_cast<uint32_t>(m_sparse.size())) {
            // Widen key to size_t BEFORE the +1 to avoid uint32 overflow; new slots = kInvalid.
            m_sparse.resize(static_cast<std::size_t>(key) + 1u, kInvalid);
        }
        uint32_t newDense = static_cast<uint32_t>(m_dense.size());
        m_sparse[key] = newDense;
        m_dense.push_back(key);
        m_values.push_back(std::move(value));
        return true;
    }

    // Pointer to the stored value, or nullptr if the key is absent. NOTE: the
    // pointer aliases m_values; a subsequent insert() may reallocate (push_back
    // growth) and invalidate all outstanding T*/refs/iterators — re-fetch after
    // any insert(). remove() doesn't grow but moves the last element into the
    // removed slot + pop_back, so treat any remove() as invalidating outstanding
    // T*/refs too (same spirit as Pool<T>::get()).
    T* get(uint32_t key) { return contains(key) ? &m_values[m_sparse[key]] : nullptr; }
    const T* get(uint32_t key) const { return contains(key) ? &m_values[m_sparse[key]] : nullptr; }

    // Remove key via swap-and-pop: move the last live element into the removed
    // dense slot and re-point its sparse entry, then drop the tail. Returns false
    // if the key was absent.
    bool remove(uint32_t key) {
        if (!contains(key)) {
            return false;
        }
        uint32_t d = m_sparse[key];
        uint32_t last = static_cast<uint32_t>(m_dense.size()) - 1u;  // safe: contains() => size >= 1
        m_dense[d]  = m_dense[last];
        m_values[d] = std::move(m_values[last]);
        m_sparse[m_dense[d]] = d;  // the moved element's key now maps to slot d — the bug-prone line; keep it unconditional
        m_sparse[key] = kInvalid;  // MUST come AFTER the re-point above (so the d==last case ends kInvalid, not d)
        m_dense.pop_back();
        m_values.pop_back();
        // On the d==last self-remove (removing the last live element), the two
        // assignments above are self-assign/self-move, but the element is
        // immediately pop_back()ed so its post-move state is never observed. The
        // statement ordering (kInvalid AFTER the re-point) makes this correct
        // with NO special-case branch — the ordering is load-bearing: do not
        // reorder, do not drop the re-point line.
        return true;
    }

    std::size_t size() const { return m_dense.size(); }
    bool empty() const { return m_dense.empty(); }

    // Clearing all three vectors is the simplest correct reset: contains()
    // range-checks m_sparse first, so an emptied m_sparse reports every key absent.
    void clear() {
        m_sparse.clear();
        m_dense.clear();
        m_values.clear();
    }

    // keys()/values() are index-parallel packed arrays for cache-friendly ECS
    // iteration — values()[i] is the value for entity keys()[i]. Non-const
    // values() lets systems mutate components in place (safe — it doesn't touch
    // m_sparse/m_dense). keys() is const-only: mutating a key would desync m_sparse.
    const std::vector<uint32_t>& keys() const { return m_dense; }
    std::vector<T>& values() { return m_values; }
    const std::vector<T>& values() const { return m_values; }

private:
    std::vector<uint32_t> m_sparse;  // key -> dense index; kInvalid for absent keys
    std::vector<uint32_t> m_dense;   // dense index -> key (packed key array)
    std::vector<T>        m_values;  // dense index -> value, index-parallel to m_dense
};

} // namespace maz::core
