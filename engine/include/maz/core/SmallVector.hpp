#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
#include <type_traits>

#include "maz/core/Assert.hpp"

namespace maz::core {

// SmallVector<T, N> — a contiguous, growable sequence with N inline (in-object,
// aligned) slots that spill to a heap allocation only once growth would exceed
// N. Modeled on llvm::SmallVector / Godot's LocalVector: the common small case
// touches no allocator, and once the vector has spilled it stays on the heap
// (clear() keeps the capacity). This trades a larger object for far fewer
// allocations when sizes are typically small.
//
// INVALIDATION: data()/operator[]/begin()/end()/front()/back() and any raw
// iterator are invalidated by any growth (push_back/emplace_back/reserve that
// spills or reallocates) and by move/assignment. Re-fetch pointers afterward.
//
// NOT thread-safe. T must be move-constructible; the copy operations
// additionally require T to be copy-constructible.
//
// EXCEPTION SAFETY: basic guarantee. Growth relocates elements via
// std::move_if_noexcept, so nothrow-move types move while throwing-move but
// copyable types copy — a throw mid-relocation therefore leaves the source
// elements intact. The inline slots are raw aligned bytes; element lifetime is
// managed manually with placement-new plus an explicit ~T() call (std::launder
// is deliberately omitted here for readability — this is the standard
// aligned-storage pattern). Self-referential push_back (pushing an element that
// aliases this same vector) is UB, exactly as with std::vector.
//
// Future refinements (mentioned, not built): count/fill and initializer_list
// constructors, and insert/erase/resize.
template <class T, std::size_t N>
class SmallVector {
    static_assert(N >= 1, "SmallVector requires N >= 1 inline slots");

public:
    bool isInline() const { return m_data == inlinePtr(); }  // pointer-identity test (exposed for tests)

    SmallVector() { m_data = inlinePtr(); m_size = 0; m_capacity = N; }

    ~SmallVector() { destroyRange(m_data, m_size); if (!isInline()) { deallocate(m_data); } }  // NEVER deallocate the inline buffer

    SmallVector(const SmallVector& other) {
        m_data = inlinePtr();
        m_size = 0;
        m_capacity = N;
        if (other.m_size > N) { growTo(other.m_size); }
        for (std::size_t i = 0; i < other.m_size; ++i) {
            ::new (static_cast<void*>(m_data + i)) T(other.m_data[i]);
        }
        m_size = other.m_size;
    }

    SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible<T>::value) {
        if (!other.isInline()) {  // HEAP SOURCE: steal wholesale, no element moves
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = other.inlinePtr();
            other.m_size = 0;
            other.m_capacity = N;  // reset source to empty-inline
        } else {  // INLINE SOURCE: cannot steal an in-object buffer; element-wise move
            m_data = inlinePtr();
            m_size = 0;
            m_capacity = N;
            for (std::size_t i = 0; i < other.m_size; ++i) {
                ::new (static_cast<void*>(m_data + i)) T(std::move(other.m_data[i]));
            }
            m_size = other.m_size;
            destroyRange(other.m_data, other.m_size);  // destroy moved-from source; leave it empty-inline
            other.m_size = 0;
        }
    }

    SmallVector& operator=(const SmallVector& other) {
        if (this == &other) { return *this; }
        SmallVector tmp(other);
        *this = std::move(tmp);
        return *this;
    }

    SmallVector& operator=(SmallVector&& other) noexcept(std::is_nothrow_move_constructible<T>::value) {
        if (this == &other) { return *this; }
        destroyRange(m_data, m_size);
        if (!isInline()) { deallocate(m_data); }
        if (!other.isInline()) {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = other.inlinePtr();
            other.m_size = 0;
            other.m_capacity = N;
        } else {
            m_data = inlinePtr();
            m_capacity = N;
            for (std::size_t i = 0; i < other.m_size; ++i) {
                ::new (static_cast<void*>(m_data + i)) T(std::move(other.m_data[i]));
            }
            m_size = other.m_size;
            destroyRange(other.m_data, other.m_size);
            other.m_size = 0;
        }
        return *this;
    }

    void push_back(const T& v) {
        if (m_size == m_capacity) { growTo(nextCapacity(m_size + 1)); }
        ::new (static_cast<void*>(m_data + m_size)) T(v);
        ++m_size;
    }

    void push_back(T&& v) {
        if (m_size == m_capacity) { growTo(nextCapacity(m_size + 1)); }
        ::new (static_cast<void*>(m_data + m_size)) T(std::move(v));
        ++m_size;
    }

    template <class... A>
    T& emplace_back(A&&... args) {
        if (m_size == m_capacity) { growTo(nextCapacity(m_size + 1)); }
        T* slot = m_data + m_size;
        ::new (static_cast<void*>(slot)) T(std::forward<A>(args)...);
        ++m_size;
        return *slot;
    }

    void pop_back() { MAZ_ASSERT(m_size > 0, "pop_back on empty SmallVector"); --m_size; m_data[m_size].~T(); }

    void clear() { destroyRange(m_data, m_size); m_size = 0; }  // capacity/heap unchanged

    void reserve(std::size_t n) { if (n > m_capacity) { growTo(n); } }

    T&       operator[](std::size_t i)       { MAZ_ASSERT(i < m_size, "index out of range"); return m_data[i]; }
    const T& operator[](std::size_t i) const { MAZ_ASSERT(i < m_size, "index out of range"); return m_data[i]; }

    T&       front()       { MAZ_ASSERT(m_size > 0, "front() on empty"); return m_data[0]; }
    const T& front() const { MAZ_ASSERT(m_size > 0, "front() on empty"); return m_data[0]; }

    T&       back()       { MAZ_ASSERT(m_size > 0, "back() on empty"); return m_data[m_size - 1]; }
    const T& back() const { MAZ_ASSERT(m_size > 0, "back() on empty"); return m_data[m_size - 1]; }

    T*       data()       { return m_data; }
    const T* data() const { return m_data; }

    T*       begin()       { return m_data; }
    T*       end()         { return m_data + m_size; }
    const T* begin() const { return m_data; }
    const T* end()   const { return m_data + m_size; }

    std::size_t size()     const { return m_size; }
    bool        empty()    const { return m_size == 0; }
    std::size_t capacity() const { return m_capacity; }

    static constexpr std::size_t inlineCapacity() { return N; }

private:
    T*       inlinePtr()       { return reinterpret_cast<T*>(m_inline); }
    const T* inlinePtr() const { return reinterpret_cast<const T*>(m_inline); }

    static T* allocate(std::size_t n) {
        MAZ_ASSERT(n != 0, "allocate(0) not expected");
        MAZ_ASSERT(sizeof(T) <= SIZE_MAX / n, "SmallVector capacity * sizeof(T) overflows size_t");
        return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t(alignof(T))));
    }

    static void deallocate(T* p) { ::operator delete(p, std::align_val_t(alignof(T))); }  // ONLY on heap pointers, NEVER inlinePtr()

    static void destroyRange(T* first, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) { first[i].~T(); }
    }

    std::size_t nextCapacity(std::size_t required) const {
        std::size_t newCap = m_capacity * 2;
        if (newCap < required) { newCap = required; }
        return newCap;
    }

    void growTo(std::size_t newCap) {
        const bool wasInline = isInline();
        T* newBlock = allocate(newCap);
        for (std::size_t i = 0; i < m_size; ++i) {
            ::new (static_cast<void*>(newBlock + i)) T(std::move_if_noexcept(m_data[i]));  // relocate; move_if_noexcept keeps basic guarantee
        }
        destroyRange(m_data, m_size);  // destroy OLD elements (moved-from or copied-from)
        if (!wasInline) { deallocate(m_data); }  // free OLD heap block; NEVER free inlinePtr()
        m_data = newBlock;
        m_capacity = newCap;
    }

    alignas(T) unsigned char m_inline[N * sizeof(T)];  // raw aligned inline storage; NOT a constructed T[]
    T*          m_data     = nullptr;  // inlinePtr() while inline, heap block after spill; set in every ctor body
    std::size_t m_size     = 0;
    std::size_t m_capacity = N;
};

} // namespace maz::core
