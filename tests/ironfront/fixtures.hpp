#pragma once
// Small hand-built worlds for the Iron Front rules tests.
#include "ironfront/Movement.hpp"

#include <string>
#include <vector>

namespace fx {
using namespace ironfront;

// A world from terrain rows; every land hex is owned by `owner`.
inline World world(const std::vector<std::string>& rows, int owner = kNeutral) {
    std::vector<std::string> owners(rows.size(), std::string(rows.empty() ? 0 : rows[0].size(), '-'));
    World w;
    w.map = Map::fromAscii(rows, owners);
    if (owner != kNeutral)
        for (int r = 0; r < w.map.height(); ++r)
            for (int c = 0; c < w.map.width(); ++c)
                if (w.map.terrain(offsetToHex(c, r)) != Terrain::Sea) w.map.setOwner(offsetToHex(c, r), owner);
    return w;
}

inline World plains(int size = 9) { return world(std::vector<std::string>(static_cast<std::size_t>(size), std::string(static_cast<std::size_t>(size), '.'))); }

inline Unit& add(World& w, UnitType t, int owner, int col, int row) {
    Unit u;
    u.id = static_cast<int>(w.units.size()) + 1;
    u.type = t;
    u.owner = owner;
    u.hex = offsetToHex(col, row);
    u.hp = stats(t).maxHp;
    w.units.push_back(u);
    return w.units.back();
}

inline bool has(const std::vector<Hex>& v, const Hex& h) {
    for (const Hex& x : v)
        if (x == h) return true;
    return false;
}
} // namespace fx
