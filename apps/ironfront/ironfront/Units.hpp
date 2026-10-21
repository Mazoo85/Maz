#pragma once

// Iron Front — the unit table. Every unit type's numbers live here, in one place, so balancing the
// game is a matter of editing this table. Values match docs/superpowers/specs/2026-10-21-ironfront-design.md.
namespace ironfront {

enum class Domain { Land, Air, Sea };

enum class UnitType : int { Infantry, Tank, Artillery, Fighter, Destroyer, Count };

constexpr int kUnitTypeCount = static_cast<int>(UnitType::Count);

struct UnitStats {
    const char* name;
    char letter; // drawn on the unit counter
    int maxHp;
    int attack;
    int defense;
    int move;   // move points per turn
    int range;  // attack range in hexes
    int vision; // sight radius in hexes
    Domain domain;
};

inline const UnitStats& stats(UnitType t) {
    static const UnitStats table[kUnitTypeCount] = {
        {"Infantry", 'I', 10, 4, 4, 1, 1, 2, Domain::Land},
        {"Tank", 'T', 12, 7, 4, 2, 1, 2, Domain::Land},
        {"Artillery", 'A', 8, 7, 2, 1, 2, 2, Domain::Land},
        {"Fighter", 'F', 8, 5, 3, 4, 1, 4, Domain::Air},
        {"Destroyer", 'D', 10, 5, 4, 3, 1, 3, Domain::Sea},
    };
    return table[static_cast<int>(t)];
}

} // namespace ironfront
