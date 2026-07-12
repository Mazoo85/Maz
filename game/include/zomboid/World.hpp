// ZOMBOID: ANCHORAGE — world data + generator.
// A tile replica of downtown/midtown Anchorage: a street grid with named
// real-world landmark buildings. Ported from js/world.js + js/worldgen.js.
#pragma once

#include <string>
#include <vector>

#include "zomboid/Math.hpp"
#include "zomboid/Tiles.hpp"

namespace zb {

constexpr int kMapW = 160;
constexpr int kMapH = 140;

struct Avenue {
    std::string name;
    int row;
};

struct Street {
    std::string name;
    int col;
};

struct Building {
    std::string name;
    int x, y, w, h;
    int doorX, doorY; // offset from top-left
    int loot;         // richness 0..3
    std::string kind; // loot-table theme
};

// Static Anchorage data (avenues, streets, landmark buildings).
const std::vector<Avenue>& avenues();
const std::vector<Street>& streets();
const std::vector<Building>& buildings();

// A lootable container baked into a building interior.
struct Container {
    int x, y;
    int loot;         // richness of the source building
    std::string name; // building name
    std::string kind; // loot-table theme
    bool opened = false;
};

struct SpawnPoint {
    int x, y;
};

// The generated world: a tile grid plus baked containers and spawn data.
struct World {
    int w = kMapW;
    int h = kMapH;
    std::vector<Tile> tiles; // row-major, size w*h
    std::vector<Container> containers;
    std::vector<SpawnPoint> spawnPoints;
    Vec2 spawn{49.0f, 36.0f}; // Town Square Park, 5th & C

    Tile at(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return Tile::Wall;
        return tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }
    void set(int x, int y, Tile t) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = t;
    }
    // Out-of-bounds counts as solid (mirrors WORLDGEN.isSolid).
    bool isSolid(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return true;
        return isSolidTile(at(x, y));
    }
};

// Bake the Anchorage data into a fresh World.
World generateWorld();

} // namespace zb
