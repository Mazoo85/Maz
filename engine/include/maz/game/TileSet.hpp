#pragma once

#include "maz/game/Tilemap.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace maz::game {

// TileSet — Godot's TileSet resource. A Tilemap holds a grid of tile IDs; a TileSet gives each ID
// MEANING: which atlas cell to DRAW it with, and what COLLISION it contributes. Godot's per-tile
// collision can be SUB-CELL (a half-height platform, a shelf), which the Tilemap's single "solid" bit
// can't express — so a level built from one grid can mix full walls with thin ledges. TileDef carries a
// None / Full / Box collision plus an atlas source cell; the free queries turn a (Tilemap, TileSet) pair
// into world-space collision boxes, a point-solidity test, and a drop-to-ground helper. Pure data +
// geometry — deterministic, unit-testable, no GPU.

struct TileDef {
    enum Collision { None, Full, Box };

    int atlasX = 0; // atlas source cell column ("which image cell draws this tile")
    int atlasY = 0; // atlas source cell row
    int collision = None;
    math::vec2 boxMin{0.0f, 0.0f}; // Box collision as fractions of the cell in [0,1]
    math::vec2 boxMax{1.0f, 1.0f};

    bool solid() const { return collision != None; }
};

class TileSet {
public:
    void define(TileId id, const TileDef& def) { m_defs[id] = def; }
    const TileDef* get(TileId id) const {
        auto it = m_defs.find(id);
        return it == m_defs.end() ? nullptr : &it->second;
    }
    bool isSolid(TileId id) const {
        const TileDef* d = get(id);
        return d && d->solid();
    }
    std::size_t size() const { return m_defs.size(); }

private:
    std::unordered_map<TileId, TileDef> m_defs;
};

// A solid tile's world-space collision box + the cell it came from.
struct TileBox {
    math::vec2 min, max;
    TileId id;
    int cx, cy;
};

// The world-space collision box a solid tile at (cx,cy) contributes: Full = the whole cell, Box = the
// sub-rect. World origin is tile (0,0)'s top-left corner, matching Tilemap's world<->tile convention.
inline TileBox tileBox(const Tilemap& map, const TileDef& def, int cx, int cy) {
    const float ts = map.tileSize();
    const math::vec2 cellMin(static_cast<float>(cx) * ts, static_cast<float>(cy) * ts);
    math::vec2 bmin = cellMin;
    math::vec2 bmax = cellMin + math::vec2(ts, ts);
    if (def.collision == TileDef::Box) {
        bmin = cellMin + def.boxMin * ts;
        bmax = cellMin + def.boxMax * ts;
    }
    return {bmin, bmax, map.at(cx, cy), cx, cy};
}

// Every solid tile's world-space collision box across the whole map (feed these to an AABB collider).
inline std::vector<TileBox> collectSolids(const Tilemap& map, const TileSet& set) {
    std::vector<TileBox> out;
    for (int cy = 0; cy < static_cast<int>(map.height()); ++cy) {
        for (int cx = 0; cx < static_cast<int>(map.width()); ++cx) {
            const TileDef* d = set.get(map.at(cx, cy));
            if (d && d->solid()) {
                out.push_back(tileBox(map, *d, cx, cy));
            }
        }
    }
    return out;
}

// Is world point p inside a solid tile (respecting sub-cell Box shapes)? Out-of-bounds reads as empty.
inline bool solidAt(const Tilemap& map, const TileSet& set, math::vec2 p) {
    const float ts = map.tileSize();
    if (ts <= 0.0f) {
        return false;
    }
    const int cx = static_cast<int>(std::floor(p.x / ts));
    const int cy = static_cast<int>(std::floor(p.y / ts));
    if (!map.inBounds(cx, cy)) {
        return false;
    }
    const TileDef* d = set.get(map.at(cx, cy));
    if (!d || !d->solid()) {
        return false;
    }
    if (d->collision == TileDef::Full) {
        return true;
    }
    const TileBox b = tileBox(map, *d, cx, cy);
    return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y;
}

// Drop straight down the column containing world-x `x`, starting at world-y `fromY`, and return the y of
// the first solid tile's TOP surface below it (its box top). Returns `maxY` if nothing is below. Handles
// sub-cell Box tiles — a ledge's top sits mid-cell, so things rest higher than on a full tile.
inline float dropY(const Tilemap& map, const TileSet& set, float x, float fromY, float maxY) {
    const float ts = map.tileSize();
    if (ts <= 0.0f) {
        return maxY;
    }
    const int cx = static_cast<int>(std::floor(x / ts));
    if (cx < 0 || cx >= static_cast<int>(map.width())) {
        return maxY;
    }
    int startCy = static_cast<int>(std::floor(fromY / ts));
    if (startCy < 0) {
        startCy = 0;
    }
    for (int cy = startCy; cy < static_cast<int>(map.height()); ++cy) {
        const TileDef* d = set.get(map.at(cx, cy));
        if (d && d->solid()) {
            const TileBox b = tileBox(map, *d, cx, cy);
            if (b.min.y >= fromY) {
                return b.min.y;
            }
        }
    }
    return maxY;
}

} // namespace maz::game
