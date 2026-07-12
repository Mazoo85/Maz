// ZOMBOID: ANCHORAGE — item database + themed loot tables (ported from js/items.js).
#pragma once

#include <limits>
#include <string>
#include <vector>

namespace zb {

class Rng;

enum class ItemType { Weapon, Food, Drink, Med, Misc };

// Sentinel for indestructible melee (JS used Infinity for fists).
constexpr int kInfiniteDurab = std::numeric_limits<int>::max();

struct ItemDef {
    std::string id;
    std::string name;
    ItemType type = ItemType::Misc;

    // Weapon stats.
    float dmg = 0.0f;
    float range = 0.0f;
    float speed = 0.0f; // attack cooldown seconds
    int durab = kInfiniteDurab;
    bool ranged = false;
    std::string ammo; // ammo item id for ranged weapons

    // Consumable effects.
    float hunger = 0.0f;  // food: reduces player hunger by this
    float thirst = 0.0f;  // drink: reduces player thirst by this
    float fatigue = 0.0f; // drink: adds to player fatigue (negative = restores)
    float mood = 0.0f;
    float heal = 0.0f;
    bool cureInfection = false;

    int stack = 0; // >0 means stackable, max stack for loot rolls
};

// Look up an item definition by id. Returns fists if unknown.
const ItemDef& itemDef(const std::string& id);
bool hasItem(const std::string& id);

// One rolled loot drop.
struct Drop {
    std::string id;
    int qty;
};

// Roll loot for a container: 1 + richness items from the theme's weighted table.
std::vector<Drop> rollLoot(int richness, const std::string& kind, Rng& rng);

} // namespace zb
