#pragma once

#include "maz/core/Pcg32.hpp"
#include "maz/math/Rect2i.hpp"
#include "maz/math/VectorInt.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

// maz::game BSP dungeon generation — the classic roguelike "rooms and corridors" layout via binary
// space partitioning. The map area is recursively split into sub-regions; each leaf gets a randomly
// sized room, and sibling regions are joined by L-shaped corridors, so the whole dungeon is one
// connected space. This is the staple generator behind Rogue/NetHack-style levels and a different flavour
// from the organic cave generator (CellularCave): sharp rectangular rooms linked by straight halls.
// Deterministic for a given seed (core::Pcg32), pure integer grid math, header-only. Godot leaves
// procedural level generation to the game, so this is a genuinely-useful utility with testable
// structural guarantees (rooms in bounds, rooms non-overlapping, all floor connected).
namespace maz::game {

// The generated map: row-major `tiles` (0 = wall, 1 = floor) plus the list of carved rooms.
struct Dungeon {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> tiles; // width*height, 0 wall / 1 floor
    std::vector<math::Rect2i> rooms;

    bool floorAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return false;
        }
        return tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)] != 0;
    }
};

// Tunable generation parameters.
struct BspDungeonParams {
    int minLeaf = 6;    // a region smaller than 2*minLeaf on both axes stops splitting
    int minRoom = 3;    // smallest room edge (must be <= minLeaf - 2)
    int maxDepth = 5;   // recursion cap
};

namespace detail {

inline void bspCarveFloor(Dungeon& d, int x, int y) {
    if (x < 0 || y < 0 || x >= d.width || y >= d.height) {
        return;
    }
    d.tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(d.width) +
            static_cast<std::size_t>(x)] = 1;
}

// Carve an L-shaped 1-wide corridor between two points (horizontal leg then vertical leg).
inline void bspCarveCorridor(Dungeon& d, math::Vector2i a, math::Vector2i b) {
    const int x0 = std::min(a.x, b.x);
    const int x1 = std::max(a.x, b.x);
    for (int x = x0; x <= x1; ++x) {
        bspCarveFloor(d, x, a.y);
    }
    const int y0 = std::min(a.y, b.y);
    const int y1 = std::max(a.y, b.y);
    for (int y = y0; y <= y1; ++y) {
        bspCarveFloor(d, b.x, y);
    }
}

inline math::Vector2i roomCenter(const math::Rect2i& r) {
    return math::Vector2i(r.position.x + r.size.x / 2, r.position.y + r.size.y / 2);
}

// Recursively split `region` (x, y, w, h). Returns the centre of a representative room in this
// subtree so the parent can connect its two children.
inline math::Vector2i bspSplit(Dungeon& d, int rx, int ry, int rw, int rh, int depth,
                               const BspDungeonParams& p, core::Pcg32& rng) {
    const bool canW = rw >= 2 * p.minLeaf;
    const bool canH = rh >= 2 * p.minLeaf;
    if (depth <= 0 || (!canW && !canH)) {
        // Leaf: carve a room inside the region, keeping a 1-cell margin.
        const int maxW = rw - 2;
        const int maxH = rh - 2;
        const int roomW = std::min(maxW, p.minRoom + static_cast<int>(rng.nextBounded(
                                              static_cast<std::uint32_t>(std::max(1, maxW - p.minRoom + 1)))));
        const int roomH = std::min(maxH, p.minRoom + static_cast<int>(rng.nextBounded(
                                              static_cast<std::uint32_t>(std::max(1, maxH - p.minRoom + 1)))));
        const int roomX = rx + 1 +
                          static_cast<int>(rng.nextBounded(
                              static_cast<std::uint32_t>(std::max(1, rw - roomW - 1))));
        const int roomY = ry + 1 +
                          static_cast<int>(rng.nextBounded(
                              static_cast<std::uint32_t>(std::max(1, rh - roomH - 1))));
        const math::Rect2i room(roomX, roomY, roomW, roomH);
        for (int y = roomY; y < roomY + roomH; ++y) {
            for (int x = roomX; x < roomX + roomW; ++x) {
                bspCarveFloor(d, x, y);
            }
        }
        d.rooms.push_back(room);
        return roomCenter(room);
    }

    bool splitVertical; // true => split the width (a vertical cut line)
    if (canW && canH) {
        splitVertical = rw >= rh; // cut the longer axis
    } else {
        splitVertical = canW;
    }

    if (splitVertical) {
        const int cut = p.minLeaf +
                        static_cast<int>(rng.nextBounded(
                            static_cast<std::uint32_t>(rw - 2 * p.minLeaf + 1)));
        const math::Vector2i ca = bspSplit(d, rx, ry, cut, rh, depth - 1, p, rng);
        const math::Vector2i cb = bspSplit(d, rx + cut, ry, rw - cut, rh, depth - 1, p, rng);
        bspCarveCorridor(d, ca, cb);
        return ca;
    }
    const int cut = p.minLeaf +
                    static_cast<int>(rng.nextBounded(
                        static_cast<std::uint32_t>(rh - 2 * p.minLeaf + 1)));
    const math::Vector2i ca = bspSplit(d, rx, ry, rw, cut, depth - 1, p, rng);
    const math::Vector2i cb = bspSplit(d, rx, ry + cut, rw, rh - cut, depth - 1, p, rng);
    bspCarveCorridor(d, ca, cb);
    return ca;
}

} // namespace detail

// Generate a BSP rooms-and-corridors dungeon of the given size, deterministic for `seed`. Returns an
// all-wall map with no rooms when the area is too small to fit even one room.
inline Dungeon generateBspDungeon(int width, int height, std::uint64_t seed,
                                  const BspDungeonParams& params = BspDungeonParams()) {
    Dungeon d;
    d.width = std::max(0, width);
    d.height = std::max(0, height);
    d.tiles.assign(static_cast<std::size_t>(d.width) * static_cast<std::size_t>(d.height), 0);
    // Need room for a bordered room: minRoom + 2 margin on each axis at minimum.
    if (d.width < params.minRoom + 2 || d.height < params.minRoom + 2) {
        return d;
    }
    core::Pcg32 rng(seed, 0xda3e39cb94b95bdbULL);
    detail::bspSplit(d, 0, 0, d.width, d.height, params.maxDepth, params, rng);
    return d;
}

} // namespace maz::game
