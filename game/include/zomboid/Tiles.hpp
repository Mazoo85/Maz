// Tile codes for the Anchorage tile map (ported from js/world.js).
#pragma once

#include <cstdint>

namespace zb {

enum class Tile : uint8_t {
    Grass = 0,
    Street = 1,
    Sidewalk = 2,
    Floor = 3,
    Wall = 4,
    Door = 5,
    Water = 6,
    Tree = 7,
    Lot = 8,
    Rail = 9,
};

// Solid tiles block movement and bullets (walls, water, trees), matching
// WORLDGEN.isSolid in the reference game.
inline bool isSolidTile(Tile t) {
    return t == Tile::Wall || t == Tile::Water || t == Tile::Tree;
}

} // namespace zb
