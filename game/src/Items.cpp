#include "zomboid/Items.hpp"

#include <cmath>
#include <unordered_map>

#include "zomboid/Rng.hpp"

namespace zb {

namespace {

// Convenience builders keep the table below readable.
ItemDef weapon(std::string id, std::string name, float dmg, float range, float speed, int durab,
               bool ranged = false, std::string ammo = {}) {
    ItemDef d;
    d.id = std::move(id);
    d.name = std::move(name);
    d.type = ItemType::Weapon;
    d.dmg = dmg;
    d.range = range;
    d.speed = speed;
    d.durab = durab;
    d.ranged = ranged;
    d.ammo = std::move(ammo);
    return d;
}

ItemDef food(std::string id, std::string name, float hunger, float mood = 0.0f) {
    ItemDef d;
    d.id = std::move(id);
    d.name = std::move(name);
    d.type = ItemType::Food;
    d.hunger = hunger;
    d.mood = mood;
    return d;
}

ItemDef drink(std::string id, std::string name, float thirst, float fatigue = 0.0f,
              float mood = 0.0f) {
    ItemDef d;
    d.id = std::move(id);
    d.name = std::move(name);
    d.type = ItemType::Drink;
    d.thirst = thirst;
    d.fatigue = fatigue;
    d.mood = mood;
    return d;
}

ItemDef med(std::string id, std::string name, float heal, bool cure = false) {
    ItemDef d;
    d.id = std::move(id);
    d.name = std::move(name);
    d.type = ItemType::Med;
    d.heal = heal;
    d.cureInfection = cure;
    return d;
}

ItemDef misc(std::string id, std::string name, int stack = 0) {
    ItemDef d;
    d.id = std::move(id);
    d.name = std::move(name);
    d.type = ItemType::Misc;
    d.stack = stack;
    return d;
}

const std::unordered_map<std::string, ItemDef>& database() {
    static const std::unordered_map<std::string, ItemDef> kItems = [] {
        std::unordered_map<std::string, ItemDef> m;
        auto add = [&m](ItemDef d) { m.emplace(d.id, std::move(d)); };

        add(weapon("fists", "Bare Fists", 8, 1.3f, 0.35f, kInfiniteDurab));
        add(weapon("branch", "Birch Branch", 14, 1.5f, 0.45f, 14));
        add(weapon("bat", "Louisville Bat", 26, 1.6f, 0.5f, 40));
        add(weapon("crowbar", "Crowbar", 30, 1.5f, 0.55f, 80));
        add(weapon("axe", "Fire Axe", 48, 1.7f, 0.7f, 60));
        add(weapon("machete", "Machete", 40, 1.6f, 0.5f, 70));
        add(weapon("pistol", "M9 Pistol", 70, 7.0f, 0.4f, 9999, true, "ammo9"));
        add(weapon("shotgun", "Mossberg 500", 120, 5.0f, 0.9f, 9999, true, "shells"));

        add(misc("ammo9", "9mm Rounds", 60));
        add(misc("shells", "12ga Shells", 24));

        add(food("cannedsalmon", "Canned Salmon", 45));
        add(food("granola", "Granola Bar", 20));
        add(food("chips", "Bag of Chips", 18));
        add(food("moosejerky", "Moose Jerky", 30));
        add(food("cannedbeans", "Canned Beans", 38));
        add(food("pizza", "Moose's Tooth Slice", 35, 8));

        add(drink("water", "Bottled Water", 40));
        add(drink("soda", "Can of Soda", 28, 0, 4));
        add(drink("coffee", "Kaladi Coffee", 20, -25));
        add(drink("energy", "Energy Drink", 18, -35));

        add(med("bandage", "Bandage", 18));
        add(med("pills", "Painkillers", 30));
        add(med("firstaid", "First Aid Kit", 55));
        add(med("antibiotics", "Antibiotics", 25, true));

        add(misc("parka", "Down Parka"));
        add(misc("flashlight", "Flashlight"));
        add(misc("map", "Anchorage Map"));
        return m;
    }();
    return kItems;
}

struct LootEntry {
    const char* id;
    int weight;
};

const std::vector<LootEntry>& lootTable(const std::string& kind) {
    static const std::unordered_map<std::string, std::vector<LootEntry>> kTables = {
        {"grocery",
         {{"cannedsalmon", 6}, {"cannedbeans", 6}, {"chips", 5}, {"water", 6}, {"soda", 5},
          {"granola", 5}, {"pizza", 2}, {"moosejerky", 3}}},
        {"food",
         {{"pizza", 6}, {"soda", 5}, {"water", 5}, {"chips", 4}, {"coffee", 4}, {"granola", 3}}},
        {"bar", {{"soda", 5}, {"water", 4}, {"energy", 4}, {"chips", 3}, {"bat", 2}}},
        {"gas",
         {{"soda", 5}, {"chips", 5}, {"water", 4}, {"granola", 4}, {"ammo9", 2}, {"flashlight", 2}}},
        {"hospital",
         {{"firstaid", 6}, {"bandage", 7}, {"pills", 6}, {"antibiotics", 4}, {"water", 3}}},
        {"police",
         {{"pistol", 4}, {"ammo9", 6}, {"shotgun", 2}, {"shells", 4}, {"bandage", 3},
          {"crowbar", 3}}},
        {"hardware",
         {{"axe", 4}, {"crowbar", 5}, {"machete", 3}, {"bat", 3}, {"flashlight", 4},
          {"branch", 3}}},
        {"outdoor",
         {{"machete", 3}, {"moosejerky", 4}, {"parka", 4}, {"water", 4}, {"shotgun", 2},
          {"shells", 4}, {"map", 3}, {"granola", 3}}},
        {"mall",
         {{"chips", 4}, {"soda", 4}, {"bat", 3}, {"bandage", 3}, {"parka", 3}, {"granola", 3},
          {"flashlight", 2}}},
        {"hotel",
         {{"water", 5}, {"granola", 4}, {"bandage", 3}, {"pills", 3}, {"chips", 3}}},
        {"default",
         {{"water", 4}, {"granola", 4}, {"chips", 4}, {"branch", 3}, {"bandage", 2}, {"soda", 3}}},
    };
    auto it = kTables.find(kind);
    if (it != kTables.end()) return it->second;
    return kTables.at("default");
}

} // namespace

bool hasItem(const std::string& id) { return database().count(id) != 0; }

const ItemDef& itemDef(const std::string& id) {
    const auto& db = database();
    auto it = db.find(id);
    if (it != db.end()) return it->second;
    return db.at("fists");
}

std::vector<Drop> rollLoot(int richness, const std::string& kind, Rng& rng) {
    const auto& table = lootTable(kind);
    int total = 0;
    for (const auto& e : table) total += e.weight;

    std::vector<Drop> drops;
    const int n = 1 + richness; // 1..4 items
    for (int i = 0; i < n; i++) {
        float r = rng.nextFloat() * static_cast<float>(total);
        for (const auto& e : table) {
            r -= static_cast<float>(e.weight);
            if (r <= 0.0f) {
                const std::string id = e.id;
                const ItemDef& base = itemDef(id);
                int qty = 1;
                if (base.stack > 0) {
                    // JS: ceil(random * stack * 0.5) + 4
                    qty = static_cast<int>(std::ceil(rng.nextFloat() *
                                                     static_cast<float>(base.stack) * 0.5f)) +
                          4;
                }
                drops.push_back(Drop{id, qty});
                break;
            }
        }
    }
    return drops;
}

} // namespace zb
