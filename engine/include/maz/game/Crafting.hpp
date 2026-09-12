#pragma once

#include <cstddef>
#include <vector>

#include "maz/game/Inventory.hpp"

// maz::game crafting system — turns a bench of recipes plus an Inventory into "combine these items to make
// that item." A Recipe lists the input item ids and quantities it consumes and the output id and quantity
// it produces; `canCraft` checks an inventory has every ingredient and room for the result, and `craft`
// atomically consumes the inputs and adds the output (leaving the inventory untouched on failure). A
// RecipeBook stores many recipes and can report which ones a given inventory can currently make — the data
// behind a crafting menu that greys out what you can't yet build. Built directly on maz::game::Inventory
// (ItemStack ids and quantities), so crafting and carrying share one item model. Godot ships no crafting
// system — games hand-roll it every time — so this is a beyond-Godot gameplay utility extending the RPG
// suite. Header-only, std-only, deterministic.
//
// Room policy: `canCraft` requires the output to fit in the inventory's CURRENT free space (partial output
// stacks + empty slots). It does not speculatively count slots that consuming the inputs would free, so a
// completely full inventory must have room for the result before crafting — a deliberate simplification
// that keeps `canCraft` and `craft` perfectly consistent.
namespace maz::game {

struct Recipe {
    int outputId = -1;
    int outputCount = 1;
    std::vector<ItemStack> inputs; // each {id, count} consumed by one craft
};

// True if `inv` holds every input in the required quantity and has room for the output.
inline bool canCraft(const Recipe& r, const Inventory& inv) {
    if (r.outputId < 0 || r.outputCount <= 0) return false;
    for (const ItemStack& in : r.inputs) {
        if (in.count <= 0) continue; // a zero-quantity input demands nothing
        if (!inv.has(in.id, in.count)) return false;
    }
    return inv.freeSpaceFor(r.outputId) >= r.outputCount;
}

// Consume the inputs and add the output if the recipe can be crafted; returns true on success. On failure
// the inventory is left exactly as it was.
inline bool craft(const Recipe& r, Inventory& inv) {
    if (!canCraft(r, inv)) return false;
    for (const ItemStack& in : r.inputs) {
        if (in.count > 0) inv.removeItem(in.id, in.count);
    }
    inv.addItem(r.outputId, r.outputCount); // guaranteed to fit (canCraft checked)
    return true;
}

class RecipeBook {
public:
    RecipeBook() = default;

    void addRecipe(const Recipe& r) { m_recipes.push_back(r); }
    void addRecipe(int outputId, int outputCount, std::vector<ItemStack> inputs) {
        m_recipes.push_back(Recipe{outputId, outputCount, std::move(inputs)});
    }

    std::size_t recipeCount() const { return m_recipes.size(); }
    const Recipe& recipe(std::size_t i) const { return m_recipes[i]; }
    const std::vector<Recipe>& recipes() const { return m_recipes; }
    void clear() { m_recipes.clear(); }

    // Indices of every recipe craftable with `inv` right now (for greying out a crafting menu).
    std::vector<int> craftable(const Inventory& inv) const {
        std::vector<int> out;
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
            if (canCraft(m_recipes[i], inv)) out.push_back(static_cast<int>(i));
        return out;
    }

    // Index of the first craftable recipe, or -1 if none.
    int firstCraftable(const Inventory& inv) const {
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
            if (canCraft(m_recipes[i], inv)) return static_cast<int>(i);
        return -1;
    }

private:
    std::vector<Recipe> m_recipes;
};

} // namespace maz::game
