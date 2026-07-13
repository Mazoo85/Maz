#pragma once

#include <cstdint>   // std::uintptr_t, SIZE_MAX
#include <cstddef>   // std::size_t, std::byte, std::max_align_t
#include <vector>

#include "maz/core/Assert.hpp"

namespace maz::core {

// A fixed-capacity bump-pointer (arena / linear) allocator. allocate() carves
// aligned byte ranges out of one contiguous buffer by advancing a cursor; there
// is no per-allocation free. Space is reclaimed wholesale via reset(), or in
// stack order via marker() / rewindTo(). allocate() returns RAW UNINITIALIZED
// memory — no constructor or destructor is EVER run by this class; use it for
// trivially-destructible objects, or pair placement-new with a manual destroy.
// reset() and rewindTo() run no destructors; every pointer handed out dangles
// once a reset()/rewindTo() rewinds past it, or once the allocator is destroyed.
// The buffer is fixed at construction and never grows. NOT thread-safe.
//
// Capacity 0 is valid: an empty buffer with remaining() == 0 where every
// non-empty allocate() returns nullptr (no UB — data() is never dereferenced).
class LinearAllocator {
public:
    using Marker = std::size_t;

    explicit LinearAllocator(std::size_t capacity) : m_buffer(capacity), m_offset(0) {}

    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
        MAZ_ASSERT((alignment & (alignment - 1)) == 0 && alignment != 0, "alignment must be a power of two");
        // Align against the REAL base address so the RETURNED pointer is aligned,
        // not merely the offset. Widen to uintptr_t BEFORE the bitwise ops.
        const std::uintptr_t base    = reinterpret_cast<std::uintptr_t>(m_buffer.data());
        const std::uintptr_t current = base + m_offset;
        const std::uintptr_t aligned = (current + (static_cast<std::uintptr_t>(alignment) - 1)) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        const std::size_t padding = static_cast<std::size_t>(aligned - current);
        // Exhaustion check ordered to avoid size_t overflow: never form m_offset+padding+size.
        if (size > m_buffer.size()) {
            return nullptr;
        }
        if (m_offset + padding > m_buffer.size() - size) {
            return nullptr;  // cursor deliberately left UNCHANGED on failure — arena stays usable
        }
        m_offset += padding + size;
        return reinterpret_cast<void*>(aligned);
    }

    // Raw uninitialized storage — does NOT construct T. Call as allocate<T>(n)
    // (explicit T selects this template; allocate(x) without <T> resolves to the
    // byte overload).
    template <class T>
    T* allocate(std::size_t count = 1) {
        if (count != 0 && sizeof(T) > SIZE_MAX / count) {
            return nullptr;  // count * sizeof(T) would overflow size_t
        }
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }

    void reset() { m_offset = 0; }  // rewind to empty; contents/destructors untouched

    Marker marker() const { return m_offset; }

    // stack-scoped free: save marker(), allocate, rewindTo(m)
    void rewindTo(Marker m) { MAZ_ASSERT(m <= m_offset, "rewindTo: marker must not be ahead of the current offset"); m_offset = m; }

    std::size_t capacity() const { return m_buffer.size(); }
    std::size_t used() const { return m_offset; }
    std::size_t remaining() const { return m_buffer.size() - m_offset; }  // invariant m_offset <= size()

private:
    std::vector<std::byte> m_buffer;   // fixed backing store, sized at construction
    std::size_t            m_offset = 0;  // bump cursor: bytes used from the base
};

} // namespace maz::core
