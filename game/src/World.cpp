#include "zomboid/World.hpp"

#include <algorithm>

namespace zb {

const std::vector<Avenue>& avenues() {
    // East/West streets (rows), north -> south.
    static const std::vector<Avenue> kAvenues = {
        {"Ship Creek", 6},
        {"1st Avenue", 14},
        {"3rd Avenue", 22},
        {"4th Avenue", 30},
        {"5th Avenue", 38},
        {"6th Avenue", 46},
        {"9th Avenue", 54},
        {"Delaney Park Strip", 62},
        {"15th Avenue", 72},
        {"Northern Lights Boulevard", 88},
        {"Benson Boulevard", 100},
        {"Tudor Road", 122},
    };
    return kAvenues;
}

const std::vector<Street>& streets() {
    // North/South streets (columns), west -> east.
    static const std::vector<Street> kStreets = {
        {"Cook Inlet", 4},
        {"L Street", 14},
        {"K Street", 24},
        {"I Street", 34},
        {"G Street", 44},
        {"E Street", 54},
        {"C Street", 66},
        {"A Street", 78},
        {"Cordova Street", 90},
        {"Gambell Street", 104},
        {"Ingra Street", 114},
        {"Lake Otis Parkway", 132},
        {"Boniface Parkway", 150},
    };
    return kStreets;
}

const std::vector<Building>& buildings() {
    // name, x, y, w, h, doorX, doorY, loot, kind
    static const std::vector<Building> kBuildings = {
        {"Hotel Captain Cook", 26, 24, 10, 5, 4, 4, 2, "hotel"},
        {"4th Avenue Theatre", 46, 24, 7, 4, 3, 3, 1, "theatre"},
        {"Anchorage City Hall", 56, 24, 6, 4, 2, 3, 1, "gov"},
        {"Egan Convention Center", 68, 24, 9, 4, 4, 3, 1, "civic"},
        {"Dena'ina Center", 26, 32, 8, 4, 3, 3, 1, "civic"},
        {"Town Square Park", 46, 32, 7, 5, 3, 0, 0, "park"},
        {"Performing Arts Center", 56, 32, 8, 5, 3, 4, 1, "civic"},
        {"Nordstrom (5th Ave Mall)", 68, 32, 9, 5, 4, 4, 3, "mall"},
        {"Anchorage Museum", 56, 40, 11, 5, 5, 0, 1, "museum"},
        {"Z.J. Loussac Library", 80, 40, 8, 5, 3, 4, 1, "gov"},
        {"Snow City Cafe", 16, 40, 5, 3, 2, 2, 2, "food"},
        {"Glacier Brewhouse", 26, 40, 5, 3, 2, 2, 2, "food"},
        {"49th State Brewing", 36, 40, 6, 3, 2, 2, 2, "food"},
        {"Carrs Aurora Village", 92, 32, 10, 6, 5, 5, 3, "grocery"},
        {"Fred Meyer", 106, 32, 12, 7, 6, 6, 3, "grocery"},
        {"Title Wave Books", 92, 42, 7, 4, 3, 3, 1, "shop"},
        {"REI Anchorage", 102, 42, 7, 4, 3, 3, 3, "outdoor"},
        {"Sullivan Arena", 100, 50, 12, 7, 6, 6, 1, "arena"},
        {"Merrill Field Hangars", 120, 24, 14, 8, 7, 7, 2, "industrial"},
        {"Providence Medical Center", 120, 92, 16, 9, 8, 8, 3, "hospital"},
        {"Alaska Native Medical Center", 120, 104, 14, 8, 7, 7, 3, "hospital"},
        {"Moose's Tooth Pub", 96, 92, 6, 4, 3, 3, 2, "food"},
        {"Bear Tooth Theatrepub", 84, 92, 6, 4, 3, 3, 2, "food"},
        {"Chilkoot Charlie's", 16, 90, 7, 4, 3, 3, 2, "bar"},
        {"Spenard Builders Supply", 16, 100, 9, 5, 4, 4, 3, "hardware"},
        {"Dimond Center Mall", 30, 118, 16, 9, 8, 8, 3, "mall"},
        {"University of Alaska Anchorage", 132, 70, 16, 10, 8, 9, 2, "campus"},
        {"Alaska Regional Hospital", 132, 56, 12, 7, 6, 6, 3, "hospital"},
        {"Anchorage Police Dept", 80, 56, 8, 5, 4, 4, 2, "police"},
        {"APD Crime Lab", 92, 56, 6, 4, 2, 3, 2, "police"},
        {"Alaska Railroad Depot", 26, 8, 10, 4, 5, 3, 1, "transit"},
        {"Westchester Lagoon Pavilion", 14, 64, 5, 3, 2, 2, 0, "park"},
        {"Bass Pro / Sportsman", 100, 100, 10, 6, 5, 5, 3, "outdoor"},
        {"Midtown Mall", 70, 92, 9, 5, 4, 4, 3, "mall"},
        {"The Lakefront Hotel", 54, 100, 9, 5, 4, 4, 2, "hotel"},
        {"Tesoro Gas Station", 60, 76, 5, 3, 2, 2, 2, "gas"},
        {"Holiday Fuel", 96, 76, 5, 3, 2, 2, 2, "gas"},
    };
    return kBuildings;
}

namespace {

bool inBounds(int x, int y) { return x >= 0 && y >= 0 && x < kMapW && y < kMapH; }

} // namespace

World generateWorld() {
    World world;
    world.w = kMapW;
    world.h = kMapH;
    world.tiles.assign(static_cast<size_t>(kMapW) * static_cast<size_t>(kMapH), Tile::Grass);

    // --- Cook Inlet water on the far west; ragged shoreline ---
    for (int y = 0; y < kMapH; y++) {
        for (int x = 0; x < 4; x++) world.set(x, y, Tile::Water);
        if ((y * 7) % 5 == 0) world.set(4, y, Tile::Water);
    }
    // Ship Creek strip across the north.
    for (int x = 4; x < kMapW; x++) {
        world.set(x, 2, Tile::Water);
        world.set(x, 3, Tile::Water);
    }
    // Chugach treeline / greenbelt on the east edge.
    for (int y = 0; y < kMapH; y++) {
        for (int x = kMapW - 6; x < kMapW; x++) {
            if ((x * 3 + y * 5) % 4 != 0)
                world.set(x, y, Tile::Tree);
            else
                world.set(x, y, Tile::Grass);
        }
    }

    // --- Avenues (horizontal streets) ---
    for (const auto& av : avenues()) {
        const int r = av.row;
        for (int x = 5; x < kMapW - 6; x++) {
            if (inBounds(x, r - 1)) world.set(x, r - 1, Tile::Sidewalk);
            if (inBounds(x, r)) world.set(x, r, Tile::Street);
            if (inBounds(x, r + 1)) world.set(x, r + 1, Tile::Street);
            if (inBounds(x, r + 2)) world.set(x, r + 2, Tile::Sidewalk);
        }
    }

    // Ship Creek railyard.
    for (int x = 5; x < kMapW - 6; x++) {
        world.set(x, 8, Tile::Rail);
        world.set(x, 9, Tile::Rail);
    }

    // --- Streets (vertical) ---
    for (const auto& st : streets()) {
        const int c = st.col;
        for (int y = 5; y < kMapH - 2; y++) {
            if (inBounds(c - 1, y)) world.set(c - 1, y, Tile::Sidewalk);
            if (inBounds(c, y)) world.set(c, y, Tile::Street);
            if (inBounds(c + 1, y)) world.set(c + 1, y, Tile::Street);
            if (inBounds(c + 2, y)) world.set(c + 2, y, Tile::Sidewalk);
        }
    }

    // --- Buildings ---
    for (const auto& b : buildings()) {
        const bool isPark = b.kind == "park";
        for (int yy = 0; yy < b.h; yy++) {
            for (int xx = 0; xx < b.w; xx++) {
                const int gx = b.x + xx, gy = b.y + yy;
                if (!inBounds(gx, gy)) continue;
                const bool edge = xx == 0 || yy == 0 || xx == b.w - 1 || yy == b.h - 1;
                if (isPark) {
                    world.set(gx, gy, (xx + yy) % 3 == 0 ? Tile::Tree : Tile::Grass);
                } else if (edge) {
                    world.set(gx, gy, Tile::Wall);
                } else {
                    world.set(gx, gy, Tile::Floor);
                }
            }
        }
        // Door + apron/parking lot in front.
        if (!isPark) {
            const int dx = b.x + b.doorX, dy = b.y + b.doorY;
            if (inBounds(dx, dy)) world.set(dx, dy, Tile::Door);
            for (int k = 1; k <= 2; k++) {
                const int ay = dy + k;
                if (inBounds(dx, ay) && world.at(dx, ay) == Tile::Grass) world.set(dx, ay, Tile::Lot);
            }
        }
        // Loot containers scattered inside; count scales with richness.
        const int nContainers = b.loot + 1;
        for (int i = 0; i < nContainers; i++) {
            const int cx = b.x + 1 + ((i * 3 + 1) % std::max(1, b.w - 2));
            const int cy = b.y + 1 + ((i * 2 + 1) % std::max(1, b.h - 2));
            if (inBounds(cx, cy) && world.at(cx, cy) == Tile::Floor) {
                world.containers.push_back(Container{cx, cy, b.loot, b.name, b.kind, false});
            }
        }
    }

    // --- Zombie spawn points: doorways + intersections ---
    for (const auto& b : buildings()) {
        world.spawnPoints.push_back(SpawnPoint{b.x + b.doorX, b.y + b.doorY + 1});
    }
    for (const auto& av : avenues()) {
        for (const auto& st : streets()) {
            world.spawnPoints.push_back(SpawnPoint{st.col, av.row});
        }
    }

    world.spawn = Vec2{49.0f, 36.0f};
    return world;
}

} // namespace zb
