#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace maz::core {

// Generational-index slot-map / object pool — the data structure behind stable, safe handles (Godot's
// RID, an ECS's entity ids). The problem it solves: you want to hand out lightweight IDs to pooled
// objects, REUSE storage when an object is freed, and still DETECT a stale ID that refers to a slot whose
// original occupant is long gone (the classic "ABA" dangling-handle bug). A `SlotMap` stores values in a
// dense-ish slot array; each slot carries a GENERATION counter. `insert` returns a `SlotHandle{index,
// generation}`; freeing a slot bumps its generation, so any handle minted before the free no longer
// matches and `get` returns null — even after the slot is reused by a brand-new object. Freed slots are
// recycled through a free list, so memory doesn't grow unbounded. Header-only, std-only, unit-testable.

struct SlotHandle {
    uint32_t index = 0;
    uint32_t generation = 0; // 0 = never handed out (a default handle is always invalid)

    bool operator==(SlotHandle o) const { return index == o.index && generation == o.generation; }
    bool operator!=(SlotHandle o) const { return !(*this == o); }
};

template <typename T>
class SlotMap {
public:
    // Insert a value and return a handle to it. Reuses a freed slot when one is available, otherwise
    // grows the array. The returned handle stays valid until the value is erased.
    SlotHandle insert(T value) {
        uint32_t idx;
        if (!freeList_.empty()) {
            idx = freeList_.back();
            freeList_.pop_back();
            slots_[idx].value = std::move(value);
            slots_[idx].occupied = true;
        } else {
            idx = static_cast<uint32_t>(slots_.size());
            slots_.push_back(Slot{std::move(value), 1u, true}); // generations start at 1
        }
        ++size_;
        return SlotHandle{idx, slots_[idx].generation};
    }

    // Is this handle still live? (In range, slot occupied, and generation matches.)
    bool contains(SlotHandle h) const {
        return h.index < slots_.size() && slots_[h.index].occupied &&
               slots_[h.index].generation == h.generation && h.generation != 0;
    }

    // Return a pointer to the value, or nullptr if the handle is stale/invalid. The pointer is valid
    // until the next insert (which may reallocate) or an erase of this slot.
    T* get(SlotHandle h) { return contains(h) ? &slots_[h.index].value : nullptr; }
    const T* get(SlotHandle h) const { return contains(h) ? &slots_[h.index].value : nullptr; }

    // Free the value behind `h`. Bumps the slot's generation (invalidating every existing handle to it)
    // and recycles the slot. Returns false if the handle was already stale (double-free safe).
    bool erase(SlotHandle h) {
        if (!contains(h)) {
            return false;
        }
        Slot& s = slots_[h.index];
        s.occupied = false;
        s.value = T{};
        ++s.generation; // any handle minted at the old generation is now stale
        freeList_.push_back(h.index);
        --size_;
        return true;
    }

    std::size_t size() const { return size_; }      // number of LIVE values
    bool empty() const { return size_ == 0; }
    std::size_t capacity() const { return slots_.size(); } // slots ever allocated (incl. free)

    void clear() {
        slots_.clear();
        freeList_.clear();
        size_ = 0;
    }

    // Visit every live value with fn(SlotHandle, T&). Order is by slot index.
    template <typename Fn>
    void forEach(Fn&& fn) {
        for (uint32_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i].occupied) {
                fn(SlotHandle{i, slots_[i].generation}, slots_[i].value);
            }
        }
    }

private:
    struct Slot {
        T value{};
        uint32_t generation = 1;
        bool occupied = false;
    };
    std::vector<Slot> slots_;
    std::vector<uint32_t> freeList_;
    std::size_t size_ = 0;
};

} // namespace maz::core
