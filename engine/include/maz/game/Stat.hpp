#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// maz::game stat / modifier system — the backbone of RPG character attributes, buffs/debuffs, and
// equipment bonuses. A Stat holds a base value plus a stack of modifiers and combines them into a final
// value in a fixed, well-defined order so results are deterministic and stack predictably: all FLAT
// bonuses add first (base + 5 + 10), then all ADDITIVE-PERCENT bonuses sum and apply once (+10% +20% =
// +30%, so xx1.3), then each MULTIPLICATIVE-PERCENT bonus applies in turn (a separate x1.5 on top). Each
// modifier carries a `source` id so a whole buff or an unequipped item can be removed in one call without
// tracking individual handles. This is the classic "flat/percent-additive/percent-multiplicative" order
// used across game frameworks; Godot ships no stat system, so this is a beyond-Godot gameplay utility.
// Header-only, std-only, deterministic (multiplicative order is commutative, so the final value is
// independent of insertion order).
namespace maz::game {

enum class ModifierType {
    Flat,         // adds a raw amount:            v += value
    PercentAdd,   // summed, then applied once:     v *= (1 + sum(value))
    PercentMult,  // applied in sequence:           v *= (1 + value)   per modifier
};

struct StatModifier {
    double value = 0.0;
    ModifierType type = ModifierType::Flat;
    int source = -1; // group id for bulk removal (a buff, an equipment slot); -1 = anonymous
};

class Stat {
public:
    Stat() = default;
    explicit Stat(double base) : m_base(base) {}

    double base() const { return m_base; }
    void setBase(double b) { m_base = b; }

    void addModifier(const StatModifier& m) { m_mods.push_back(m); }
    void addModifier(double value, ModifierType type, int source = -1) {
        m_mods.push_back({value, type, source});
    }

    // Remove every modifier tagged with `source`. Returns true if any were removed.
    bool removeModifiersFromSource(int source) {
        const std::size_t before = m_mods.size();
        m_mods.erase(std::remove_if(m_mods.begin(), m_mods.end(),
                                    [source](const StatModifier& m) { return m.source == source; }),
                     m_mods.end());
        return m_mods.size() != before;
    }

    void clearModifiers() { m_mods.clear(); }
    std::size_t modifierCount() const { return m_mods.size(); }
    const std::vector<StatModifier>& modifiers() const { return m_mods; }

    // The final value after applying every modifier in the standard flat -> percent-add -> percent-mult
    // order.
    double value() const {
        double v = m_base;
        for (const StatModifier& m : m_mods)
            if (m.type == ModifierType::Flat) v += m.value;
        double sumPercentAdd = 0.0;
        for (const StatModifier& m : m_mods)
            if (m.type == ModifierType::PercentAdd) sumPercentAdd += m.value;
        v *= (1.0 + sumPercentAdd);
        for (const StatModifier& m : m_mods)
            if (m.type == ModifierType::PercentMult) v *= (1.0 + m.value);
        return v;
    }

    // The final value clamped to [lo, hi] (e.g. keep health at or above 0, movement speed capped).
    double valueClamped(double lo, double hi) const {
        const double v = value();
        return v < lo ? lo : (v > hi ? hi : v);
    }

private:
    double m_base = 0.0;
    std::vector<StatModifier> m_mods;
};

} // namespace maz::game
