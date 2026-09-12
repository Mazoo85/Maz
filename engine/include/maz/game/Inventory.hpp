#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::game slot-based stackable inventory — the backbone of loot bags, chests, hotbars, and shop
// stock. An Inventory is a fixed array of slots; each slot is either empty or holds a stack of one item
// type up to a per-inventory stack limit (Minecraft's 64, an RPG's 99, etc.). Items are referred to by an
// integer id (index into the game's own item database). Adding items fills existing partial stacks of the
// same id first, then spills into empty slots, so a scattered inventory naturally consolidates; anything
// that does not fit is reported back as leftover. Removing pulls from every stack of that id until the
// requested amount is satisfied. Godot ships no inventory system — every game hand-rolls one — so this is
// a beyond-Godot gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

// One inventory slot: an item id and how many of it are stacked here. A slot is EMPTY when the count is 0
// (in which case the id is meaningless and normalised to -1). Item ids are game-defined; any id >= 0 is
// valid.
struct ItemStack {
    int id = -1;
    int count = 0;

    bool empty() const { return count <= 0; }
};

class Inventory {
public:
    Inventory() = default;
    // `slots` slots, each stacking up to `maxStack` of a single item id (both clamped to >= 1).
    explicit Inventory(std::size_t slots, int maxStack = 99)
        : m_slots(slots == 0 ? 1 : slots), m_maxStack(maxStack < 1 ? 1 : maxStack) {}

    std::size_t slotCount() const { return m_slots.size(); }
    int maxStack() const { return m_maxStack; }
    const ItemStack& slot(std::size_t i) const { return m_slots[i]; }
    const std::vector<ItemStack>& slots() const { return m_slots; }

    // Add `count` of item `id`, stacking into existing partial stacks of the same id first, then empty
    // slots. Returns the leftover that did not fit (0 == everything was added). No-op for id < 0 or
    // count <= 0.
    int addItem(int id, int count) {
        if (id < 0 || count <= 0) return count > 0 ? count : 0;
        int remaining = count;
        // Pass 1: top up existing stacks of this id.
        for (ItemStack& s : m_slots) {
            if (remaining == 0) break;
            if (s.count > 0 && s.id == id && s.count < m_maxStack) {
                const int room = m_maxStack - s.count;
                const int add = room < remaining ? room : remaining;
                s.count += add;
                remaining -= add;
            }
        }
        // Pass 2: fill empty slots.
        for (ItemStack& s : m_slots) {
            if (remaining == 0) break;
            if (s.count <= 0) {
                const int add = m_maxStack < remaining ? m_maxStack : remaining;
                s.id = id;
                s.count = add;
                remaining -= add;
            }
        }
        return remaining;
    }

    // Remove up to `count` of item `id`, drawing from every stack of that id. Returns the number actually
    // removed (< count when the inventory held fewer). Emptied slots become free.
    int removeItem(int id, int count) {
        if (id < 0 || count <= 0) return 0;
        int removed = 0;
        for (ItemStack& s : m_slots) {
            if (removed == count) break;
            if (s.count > 0 && s.id == id) {
                const int need = count - removed;
                const int take = s.count < need ? s.count : need;
                s.count -= take;
                removed += take;
                if (s.count == 0) s.id = -1;
            }
        }
        return removed;
    }

    // Total quantity of item `id` across all slots.
    int count(int id) const {
        int total = 0;
        for (const ItemStack& s : m_slots)
            if (s.count > 0 && s.id == id) total += s.count;
        return total;
    }

    bool has(int id, int count = 1) const { return this->count(id) >= count; }

    // How many more of item `id` the inventory could still accept right now (partial stacks + empty
    // slots). addItem(id, freeSpaceFor(id)) always returns 0 leftover.
    int freeSpaceFor(int id) const {
        if (id < 0) return 0;
        long long room = 0;
        for (const ItemStack& s : m_slots) {
            if (s.count <= 0)
                room += m_maxStack; // empty slot
            else if (s.id == id && s.count < m_maxStack)
                room += (m_maxStack - s.count);
        }
        return room > 0x7fffffffLL ? 0x7fffffff : static_cast<int>(room);
    }

    // Index of the first empty slot, or -1 if none.
    int firstEmptySlot() const {
        for (std::size_t i = 0; i < m_slots.size(); ++i)
            if (m_slots[i].count <= 0) return static_cast<int>(i);
        return -1;
    }

    bool isFull() const {
        for (const ItemStack& s : m_slots)
            if (s.count <= 0 || s.count < m_maxStack) return false;
        return true;
    }

    bool isEmpty() const {
        for (const ItemStack& s : m_slots)
            if (s.count > 0) return false;
        return true;
    }

    // Swap the contents of two slots (reorganising the bag). Out-of-range indices are ignored.
    void swapSlots(std::size_t a, std::size_t b) {
        if (a >= m_slots.size() || b >= m_slots.size() || a == b) return;
        const ItemStack tmp = m_slots[a];
        m_slots[a] = m_slots[b];
        m_slots[b] = tmp;
    }

    void clear() {
        for (ItemStack& s : m_slots) {
            s.id = -1;
            s.count = 0;
        }
    }

private:
    std::vector<ItemStack> m_slots;
    int m_maxStack = 99;
};

} // namespace maz::game
