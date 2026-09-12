#pragma once

#include <cmath>
#include <cstdint>

// maz::game combat damage resolver — the "how much damage actually lands" math behind every attack. Given
// a raw hit amount and the defender's mitigation (scaling armor, a fractional resistance to the damage
// type, and optional flat reduction) plus an optional critical-hit multiplier, `resolveDamage` returns the
// final integer damage and whether the blow crit or was fully blocked. Armor uses the classic
// diminishing-returns curve `100 / (100 + armor)` (100 armor halves damage, 200 armor thirds it — never
// negative), resistance scales by `(1 - resist)`, and a `True` damage type bypasses all mitigation. Crit
// is a caller-supplied flag (roll it with the engine's Pcg32), so the resolver stays pure and
// deterministic. This is combat math Godot has no notion of — it ships no damage/combat system — so this
// is a beyond-Godot gameplay utility that pairs with the stat and status-effect systems. Header-only,
// std-only, deterministic.
namespace maz::game {

enum class DamageType {
    Physical, // mitigated by armor + resistance + flat reduction
    Magical,  // same pipeline (a distinct type so games can carry separate resist values)
    True,     // ignores all mitigation
};

struct DamageInfo {
    double amount = 0.0;          // raw incoming damage before mitigation
    DamageType type = DamageType::Physical;
    double armor = 0.0;           // scaling armor (diminishing returns); negative treated as 0
    double resist = 0.0;          // fractional resistance to this type, clamped to [0,1]
    double flatReduction = 0.0;   // flat amount subtracted after scaling; negative treated as 0
    double critMultiplier = 2.0;  // applied to the raw amount when isCrit
    bool isCrit = false;
};

struct DamageResult {
    long long damage = 0;      // final damage dealt (rounded, never negative)
    bool crit = false;         // whether the hit was a critical
    bool fullyBlocked = false; // a real hit (amount > 0) that mitigation reduced to 0
};

// The armor mitigation multiplier in [0,1]: 100 / (100 + max(0, armor)).
inline double armorMultiplier(double armor) {
    const double a = armor < 0.0 ? 0.0 : armor;
    return 100.0 / (100.0 + a);
}

inline DamageResult resolveDamage(const DamageInfo& in) {
    DamageResult out;
    out.crit = in.isCrit;

    double dmg = in.amount < 0.0 ? 0.0 : in.amount;
    if (in.isCrit) {
        const double mult = in.critMultiplier < 0.0 ? 0.0 : in.critMultiplier;
        dmg *= mult;
    }

    if (in.type != DamageType::True) {
        dmg *= armorMultiplier(in.armor);
        double resist = in.resist;
        if (resist < 0.0) resist = 0.0;
        if (resist > 1.0) resist = 1.0;
        dmg *= (1.0 - resist);
        const double flat = in.flatReduction < 0.0 ? 0.0 : in.flatReduction;
        dmg -= flat;
    }

    if (dmg < 0.0) dmg = 0.0;
    out.damage = static_cast<long long>(std::llround(dmg));
    out.fullyBlocked = (out.damage == 0 && in.amount > 0.0);
    return out;
}

} // namespace maz::game
