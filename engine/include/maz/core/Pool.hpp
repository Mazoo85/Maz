#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <utility>  // std::move

namespace maz::core {

// A generational Handle plus an object Pool that owns T instances behind those
// handles. The pairing lets a caller hold a lightweight, copyable reference and
// still detect when the referent has been destroyed (and its slot recycled) —
// the classic stale-reference guard used by resource/entity systems.

// An opaque, generational reference into a Pool<T>. Cheap to copy, compare, and
// store; carries no pointer, so it stays valid across Pool growth.
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;  // 0 == null/never-issued sentinel; live generations are >= 1

    // Both fields must match: two handles into the same slot but different
    // generations (i.e. across a destroy+reuse) are NOT equal.
    constexpr bool operator==(const Handle& o) const {
        return index == o.index && generation == o.generation;
    }
    constexpr bool operator!=(const Handle& o) const { return !(*this == o); }

    constexpr bool isNull() const { return generation == 0; }
    static constexpr Handle null() { return Handle{}; }  // index 0, generation 0

    // Handle is intentionally non-templated for simplicity. A type-tagged
    // variant (Handle<T>) that prevents mixing handles from different pools at
    // compile time is a possible future refinement — not built here.
};

// A slot-based object pool. Handles issued by create() remain resolvable until
// the object is destroy()ed; a later create() may recycle the freed slot, at
// which point the old handle becomes stale and is rejected by valid()/get().
template <class T>
class Pool {
public:
    // Create a copy of v in a free (or freshly grown) slot and return its handle.
    Handle create(const T& v) {
        if (!freeIndices.empty()) {
            uint32_t i = freeIndices.back();
            freeIndices.pop_back();
            // Reuse: the slot already holds the current (post-destroy bumped)
            // generation; create must NOT touch it. The issued generation is
            // simply the slot's current one.
            slots[i].value = v;
            slots[i].alive = true;
            ++m_count;
            return Handle{i, slots[i].generation};
        }
        uint32_t i = static_cast<uint32_t>(slots.size());
        slots.push_back(Slot{ v, 1u, true });  // fresh slots start at generation 1
        ++m_count;
        return Handle{i, 1u};
    }

    // Move-construct overload; same slot/generation rules as the copy version.
    Handle create(T&& v) {
        if (!freeIndices.empty()) {
            uint32_t i = freeIndices.back();
            freeIndices.pop_back();
            slots[i].value = std::move(v);
            slots[i].alive = true;
            ++m_count;
            return Handle{i, slots[i].generation};
        }
        uint32_t i = static_cast<uint32_t>(slots.size());
        slots.push_back(Slot{ std::move(v), 1u, true });
        ++m_count;
        return Handle{i, 1u};
    }

    // A handle is valid iff it is non-null, in range, points at a live slot, and
    // its generation matches the slot's current generation (the stale check).
    bool valid(Handle h) const {
        return h.generation != 0
            && h.index < static_cast<uint32_t>(slots.size())
            && slots[h.index].alive
            && slots[h.index].generation == h.generation;
    }

    // Pointer to the stored object, or nullptr if the handle is stale/invalid.
    // NOTE: the pointer aliases the internal slot vector; a subsequent create()
    // that grows the pool may reallocate and invalidate it. Handles survive
    // growth, but a raw T* from get() must be re-fetched after any create().
    T* get(Handle h) { return valid(h) ? &slots[h.index].value : nullptr; }
    const T* get(Handle h) const { return valid(h) ? &slots[h.index].value : nullptr; }

    // Destroy the referent and recycle its slot. Bumps the slot's generation so
    // every outstanding handle to it becomes stale; returns false for a
    // stale/invalid handle (including a double destroy).
    bool destroy(Handle h) {
        if (!valid(h)) {
            return false;
        }
        Slot& s = slots[h.index];
        s.alive = false;
        ++s.generation;
        if (s.generation == 0) {
            s.generation = 1;  // skip 0 on wraparound — it is the null sentinel
        }
        s.value = T{};  // release any resources the object held
        freeIndices.push_back(h.index);
        --m_count;
        return true;
    }

    std::size_t size() const { return m_count; }          // live object count
    std::size_t capacity() const { return slots.size(); }  // total slots ever allocated

    void clear() {
        slots.clear();
        freeIndices.clear();
        m_count = 0;
    }

private:
    struct Slot {
        T value{};
        uint32_t generation = 1;  // live generations start at 1; 0 is reserved as null
        bool alive = false;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeIndices;  // free-list of recyclable slot indices
    std::size_t m_count = 0;            // number of currently live objects
};

} // namespace maz::core
