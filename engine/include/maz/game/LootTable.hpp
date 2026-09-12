#pragma once

#include <cstddef>
#include <vector>

#include "maz/core/Pcg32.hpp"

// maz::game weighted loot table — the drop system behind chests, defeated enemies, and treasure rolls.
// A LootTable is a list of entries, each an item id with a relative weight and a quantity range; rolling
// picks ONE entry with probability proportional to its weight and yields a random count within that
// entry's [minCount, maxCount]. Weights are relative (a weight-3 entry is three times as likely as a
// weight-1 one), so designers never have to make them sum to 1. An entry with item id < 0 models a
// "nothing dropped" outcome (a blank on the wheel): it can win the roll, and the resulting drop reports
// empty(). Rolls are driven by the engine's deterministic core::Pcg32, so a given seed reproduces the
// exact same loot — essential for replays and shareable seeds. Godot ships no loot-table resource — games
// hand-roll weighted drops every time — so this is a beyond-Godot gameplay utility. Header-only, std-only.
namespace maz::game {

struct LootEntry {
    int id = -1;         // item id to drop; < 0 means a "no drop" outcome
    double weight = 1.0; // relative selection weight; <= 0 is never chosen
    int minCount = 1;    // inclusive low end of the drop quantity
    int maxCount = 1;    // inclusive high end of the drop quantity
};

struct LootDrop {
    int id = -1;
    int count = 0;

    // True when nothing usable dropped (a "no drop" entry won, or the table was empty).
    bool empty() const { return id < 0 || count <= 0; }
};

class LootTable {
public:
    LootTable() = default;

    void addEntry(const LootEntry& e) { m_entries.push_back(e); }
    void addEntry(int id, double weight, int minCount = 1, int maxCount = 1) {
        m_entries.push_back({id, weight, minCount, maxCount});
    }

    std::size_t entryCount() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }
    const std::vector<LootEntry>& entries() const { return m_entries; }
    void clear() { m_entries.clear(); }

    // Sum of the positive weights (the denominator of the probabilities). Non-positive weights and
    // absent entries contribute nothing.
    double totalWeight() const {
        double total = 0.0;
        for (const LootEntry& e : m_entries)
            if (e.weight > 0.0) total += e.weight;
        return total;
    }

    // Roll once: choose an entry by weight, then a uniform count within its range. Returns an empty drop
    // (id -1, count 0) when the table has no entry with positive weight.
    LootDrop roll(core::Pcg32& rng) const {
        const double total = totalWeight();
        if (total <= 0.0) return LootDrop{};
        // Draw the threshold BEFORE any count draw so the RNG advance is well-defined.
        const double r = static_cast<double>(rng.nextFloat()) * total;
        double acc = 0.0;
        const LootEntry* chosen = nullptr;
        for (const LootEntry& e : m_entries) {
            if (e.weight <= 0.0) continue;
            acc += e.weight;
            if (r < acc) {
                chosen = &e;
                break;
            }
        }
        if (chosen == nullptr) {
            // Float rounding at the very top end: fall back to the last positive-weight entry.
            for (const LootEntry& e : m_entries)
                if (e.weight > 0.0) chosen = &e;
        }
        if (chosen == nullptr) return LootDrop{};
        int lo = chosen->minCount < 0 ? 0 : chosen->minCount;
        int hi = chosen->maxCount < 0 ? 0 : chosen->maxCount;
        if (hi < lo) {
            const int t = lo;
            lo = hi;
            hi = t;
        }
        const int count = rng.range(lo, hi);
        return LootDrop{chosen->id, count};
    }

    // Roll `rolls` independent times. Empty ("no drop") results are included so the caller sees each
    // roll's outcome; filter on !drop.empty() to keep only real items.
    std::vector<LootDrop> rollMany(core::Pcg32& rng, int rolls) const {
        std::vector<LootDrop> out;
        if (rolls <= 0) return out;
        out.reserve(static_cast<std::size_t>(rolls));
        for (int i = 0; i < rolls; ++i) out.push_back(roll(rng));
        return out;
    }

private:
    std::vector<LootEntry> m_entries;
};

} // namespace maz::game
