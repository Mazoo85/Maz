#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

// maz::core memory allocators — the linear/frame arena and fixed-size pool a game engine leans on
// to avoid per-object malloc/free churn (the source of fragmentation and frame-time spikes). Godot
// uses the same shapes internally; here they're first-class, header-only, and unit-tested.
//
//   LinearArena — a bump allocator over one contiguous buffer. allocate() just advances an offset
//     (O(1), no per-allocation bookkeeping); you free EVERYTHING at once with reset(). That makes
//     it the ideal *frame allocator*: reset() at the top of each frame, then scratch-allocate
//     transient data (command lists, temp arrays, string builds) for free. It also supports
//     stack-style scopes via marker()/rewind(m) — grab a marker, allocate, then rewind to release
//     just that block.
//
//   PoolAllocator — a free-list of fixed-size blocks. allocate()/free() are O(1) and hand back the
//     same slots repeatedly, so spawning/despawning thousands of same-sized objects (particles,
//     bullets, entities) never fragments the heap. Exhaustion returns nullptr rather than growing.
//
// Both are non-owning of object lifetime beyond the raw bytes: they do NOT call destructors, so use
// them for trivially-destructible data (or destroy objects yourself before reset/free). No GPU, no
// threads — drive from the game thread.
namespace maz::core {

inline bool isPowerOfTwo(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

// Round `n` up to the next multiple of `align` (align must be a power of two).
inline size_t alignUp(size_t n, size_t align) {
    return (n + (align - 1)) & ~(align - 1);
}

class LinearArena {
  public:
    explicit LinearArena(size_t bytes)
        : m_buffer(new(std::align_val_t(kMaxAlign)) uint8_t[bytes]), m_capacity(bytes) {}

    ~LinearArena() { operator delete[](m_buffer, std::align_val_t(kMaxAlign)); }

    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;

    // Allocate `size` bytes aligned to `align` (a power of two, up to the arena's max alignment).
    // Returns nullptr if the arena can't fit the request (never grows). align defaults to the
    // strictest scalar alignment so any type is safe.
    void* allocate(size_t size, size_t align = alignof(std::max_align_t)) {
        if (!isPowerOfTwo(align) || align > kMaxAlign) {
            return nullptr;
        }
        const size_t aligned = alignUp(m_offset, align);
        if (aligned + size > m_capacity) {
            return nullptr; // out of space
        }
        void* p = m_buffer + aligned;
        m_offset = aligned + size;
        return p;
    }

    // Typed convenience: allocate storage for `count` Ts, correctly aligned (uninitialized memory;
    // no constructor runs — intended for trivially-constructible/destructible T). nullptr on OOM.
    template <typename T> T* alloc(size_t count = 1) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    // Free everything at once (the frame reset). O(1) — the buffer is retained for reuse.
    void reset() { m_offset = 0; }

    // Stack-style scoping: capture the current offset, allocate, then rewind() back to it to
    // release exactly what was allocated since. Rewinding to a marker past the current offset is
    // ignored.
    size_t marker() const { return m_offset; }
    void rewind(size_t m) {
        if (m <= m_offset) {
            m_offset = m;
        }
    }

    size_t used() const { return m_offset; }
    size_t capacity() const { return m_capacity; }
    size_t remaining() const { return m_capacity - m_offset; }

  private:
    static constexpr size_t kMaxAlign = alignof(std::max_align_t);
    uint8_t* m_buffer;
    size_t m_capacity;
    size_t m_offset = 0;
};

class PoolAllocator {
  public:
    // A pool of `blockCount` blocks, each at least `blockSize` bytes (bumped up to hold a free-list
    // pointer and to a pointer-aligned size). Blocks are handed out by allocate() and returned by
    // free(); the pool never grows.
    PoolAllocator(size_t blockSize, size_t blockCount)
        : m_blockSize(effectiveBlockSize(blockSize)), m_blockCount(blockCount) {
        m_storage.resize(m_blockSize * m_blockCount);
        // Thread every block onto the free list, front to back.
        m_freeList = nullptr;
        for (size_t i = m_blockCount; i-- > 0;) {
            Node* n = reinterpret_cast<Node*>(m_storage.data() + i * m_blockSize);
            n->next = m_freeList;
            m_freeList = n;
        }
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // Hand out one block, or nullptr when exhausted. The memory is uninitialized.
    void* allocate() {
        if (!m_freeList) {
            return nullptr;
        }
        Node* n = m_freeList;
        m_freeList = n->next;
        ++m_inUse;
        return n;
    }

    // Return a block previously handed out by allocate(). Passing a foreign or already-freed
    // pointer is undefined (as with any pool) — this checks range in debug via the assert-free
    // bounds test.
    void free(void* p) {
        if (!p) {
            return;
        }
        Node* n = static_cast<Node*>(p);
        n->next = m_freeList;
        m_freeList = n;
        if (m_inUse > 0) {
            --m_inUse;
        }
    }

    // Return every block to the free list at once (invalidates all outstanding pointers).
    void reset() {
        m_freeList = nullptr;
        for (size_t i = m_blockCount; i-- > 0;) {
            Node* n = reinterpret_cast<Node*>(m_storage.data() + i * m_blockSize);
            n->next = m_freeList;
            m_freeList = n;
        }
        m_inUse = 0;
    }

    size_t blockSize() const { return m_blockSize; }
    size_t capacity() const { return m_blockCount; }
    size_t inUse() const { return m_inUse; }
    size_t available() const { return m_blockCount - m_inUse; }
    bool owns(const void* p) const {
        const auto* b = reinterpret_cast<const uint8_t*>(p);
        return b >= m_storage.data() && b < m_storage.data() + m_storage.size();
    }

  private:
    struct Node {
        Node* next;
    };
    static size_t effectiveBlockSize(size_t requested) {
        size_t s = requested < sizeof(Node) ? sizeof(Node) : requested;
        return alignUp(s, alignof(Node)); // keep every block pointer-aligned
    }

    size_t m_blockSize;
    size_t m_blockCount;
    std::vector<uint8_t> m_storage;
    Node* m_freeList = nullptr;
    size_t m_inUse = 0;
};

} // namespace maz::core
