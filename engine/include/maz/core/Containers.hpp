#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <vector>

// maz::core containers — the two data structures a game engine reaches for constantly but the STL
// doesn't ship: a small-buffer vector and a sparse set.
//
//   SmallVector<T, N> — a dynamic array that keeps its first N elements INLINE (no heap allocation)
//     and only spills to the heap when it grows past N. Most engine lists are tiny and short-lived
//     (a node's children, contacts on a body, hits from a query), so keeping them inline eliminates
//     the malloc/free per list — the single biggest source of allocator churn. Same push_back /
//     indexing / range-for API as std::vector.
//
//   SparseSet<T> — maps integer keys to values with O(1) insert, remove, and lookup AND a densely-
//     packed value array you can iterate with no holes. Removal is a swap-with-last ("swap-erase"),
//     so iteration stays cache-friendly. This is the backbone of an archetype-free ECS component
//     store and any "set of entity ids with data" — exactly what Godot's servers keep internally.
//
// Header-only, no GPU, no threads.
namespace maz::core {

template <typename T, size_t N> class SmallVector {
  public:
    SmallVector() : m_data(inlinePtr()) {}
    SmallVector(const SmallVector& o) : m_data(inlinePtr()) {
        reserve(o.m_size);
        for (size_t i = 0; i < o.m_size; ++i) {
            new (&m_data[i]) T(o.m_data[i]);
        }
        m_size = o.m_size;
    }
    SmallVector(SmallVector&& o) noexcept : m_data(inlinePtr()) { moveFrom(std::move(o)); }
    SmallVector& operator=(const SmallVector& o) {
        if (this != &o) {
            clear();
            reserve(o.m_size);
            for (size_t i = 0; i < o.m_size; ++i) {
                new (&m_data[i]) T(o.m_data[i]);
            }
            m_size = o.m_size;
        }
        return *this;
    }
    SmallVector& operator=(SmallVector&& o) noexcept {
        if (this != &o) {
            reset();
            m_data = inlinePtr();
            moveFrom(std::move(o));
        }
        return *this;
    }
    ~SmallVector() { reset(); }

    void push_back(const T& v) {
        ensure(m_size + 1);
        new (&m_data[m_size]) T(v);
        ++m_size;
    }
    void push_back(T&& v) {
        ensure(m_size + 1);
        new (&m_data[m_size]) T(std::move(v));
        ++m_size;
    }
    template <typename... A> T& emplace_back(A&&... a) {
        ensure(m_size + 1);
        T* p = new (&m_data[m_size]) T(std::forward<A>(a)...);
        ++m_size;
        return *p;
    }
    void pop_back() {
        if (m_size > 0) {
            --m_size;
            m_data[m_size].~T();
        }
    }
    void clear() {
        for (size_t i = 0; i < m_size; ++i) {
            m_data[i].~T();
        }
        m_size = 0;
    }
    void reserve(size_t n) {
        if (n > m_cap) {
            grow(n);
        }
    }

    T& operator[](size_t i) { return m_data[i]; }
    const T& operator[](size_t i) const { return m_data[i]; }
    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }
    T& front() { return m_data[0]; }

    size_t size() const { return m_size; }
    size_t capacity() const { return m_cap; }
    bool empty() const { return m_size == 0; }
    bool isInline() const { return m_data == inlinePtr(); }

    T* begin() { return m_data; }
    T* end() { return m_data + m_size; }
    const T* begin() const { return m_data; }
    const T* end() const { return m_data + m_size; }

  private:
    T* inlinePtr() { return reinterpret_cast<T*>(m_inline); }
    const T* inlinePtr() const { return reinterpret_cast<const T*>(m_inline); }

    void ensure(size_t need) {
        if (need > m_cap) {
            size_t nc = m_cap * 2;
            if (nc < need) {
                nc = need;
            }
            grow(nc);
        }
    }
    void grow(size_t newCap) {
        T* nd = static_cast<T*>(::operator new(sizeof(T) * newCap));
        for (size_t i = 0; i < m_size; ++i) {
            new (&nd[i]) T(std::move(m_data[i]));
            m_data[i].~T();
        }
        if (!isInline()) {
            ::operator delete(m_data);
        }
        m_data = nd;
        m_cap = newCap;
    }
    void reset() {
        clear();
        if (!isInline()) {
            ::operator delete(m_data);
            m_data = inlinePtr();
            m_cap = N;
        }
    }
    void moveFrom(SmallVector&& o) {
        if (!o.isInline()) {
            // Steal the heap buffer wholesale.
            m_data = o.m_data;
            m_cap = o.m_cap;
            m_size = o.m_size;
            o.m_data = o.inlinePtr();
            o.m_cap = N;
            o.m_size = 0;
        } else {
            // Inline source: move each element into our inline storage.
            for (size_t i = 0; i < o.m_size; ++i) {
                new (&m_data[i]) T(std::move(o.m_data[i]));
            }
            m_size = o.m_size;
            o.clear();
        }
    }

    alignas(T) unsigned char m_inline[sizeof(T) * N];
    T* m_data;
    size_t m_size = 0;
    size_t m_cap = N;
};

template <typename T> class SparseSet {
  public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu;

    // Pre-size the sparse directory for keys up to maxKey (optional; insert grows it as needed).
    void reserve(uint32_t maxKey) {
        if (m_sparse.size() <= maxKey) {
            m_sparse.resize(maxKey + 1, kInvalid);
        }
    }

    bool contains(uint32_t key) const { return key < m_sparse.size() && m_sparse[key] != kInvalid; }

    // Insert (or overwrite) the value for `key`. O(1).
    void insert(uint32_t key, T value) {
        if (key >= m_sparse.size()) {
            m_sparse.resize(key + 1, kInvalid);
        }
        if (m_sparse[key] != kInvalid) {
            m_denseVals[m_sparse[key]] = std::move(value);
            return;
        }
        m_sparse[key] = static_cast<uint32_t>(m_denseKeys.size());
        m_denseKeys.push_back(key);
        m_denseVals.push_back(std::move(value));
    }

    // Pointer to the value for `key`, or nullptr if absent. Invalidated by insert/remove.
    T* get(uint32_t key) {
        if (!contains(key)) {
            return nullptr;
        }
        return &m_denseVals[m_sparse[key]];
    }
    const T* get(uint32_t key) const {
        if (!contains(key)) {
            return nullptr;
        }
        return &m_denseVals[m_sparse[key]];
    }

    // Remove `key` by swapping its slot with the last dense element (O(1), keeps the arrays
    // packed). Returns true if the key was present.
    bool remove(uint32_t key) {
        if (!contains(key)) {
            return false;
        }
        const uint32_t idx = m_sparse[key];
        const uint32_t lastKey = m_denseKeys.back();
        m_denseKeys[idx] = lastKey;
        m_denseVals[idx] = std::move(m_denseVals.back());
        m_sparse[lastKey] = idx;
        m_denseKeys.pop_back();
        m_denseVals.pop_back();
        m_sparse[key] = kInvalid;
        return true;
    }

    void clear() {
        for (uint32_t k : m_denseKeys) {
            m_sparse[k] = kInvalid;
        }
        m_denseKeys.clear();
        m_denseVals.clear();
    }

    size_t size() const { return m_denseKeys.size(); }
    bool empty() const { return m_denseKeys.empty(); }

    // Dense, hole-free views for iteration (keys[i] owns values[i]).
    const std::vector<uint32_t>& keys() const { return m_denseKeys; }
    std::vector<T>& values() { return m_denseVals; }
    const std::vector<T>& values() const { return m_denseVals; }

  private:
    std::vector<uint32_t> m_sparse;    // key -> dense index (or kInvalid)
    std::vector<uint32_t> m_denseKeys; // packed keys
    std::vector<T> m_denseVals;        // packed values, parallel to m_denseKeys
};

} // namespace maz::core
