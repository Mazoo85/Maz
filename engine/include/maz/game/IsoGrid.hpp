#pragma once

#include "maz/math/Math.hpp"      // vec2
#include "maz/math/VectorInt.hpp" // Vector2i

#include <cmath>
#include <vector>

// maz::game isometric tile-grid math — the diamond ("2:1") coordinate conversions behind Godot's
// TileMap Isometric layout and countless iso RPG/strategy games. A cell (col, row) maps to the pixel
// centre of its diamond and back; screenToTile rounds a pixel position to the cell under it. Tile
// width/height are the full diamond footprint in pixels (Godot's tile_size). Pure value math over
// Vector2i / vec2 — header-only, deterministic, exactly unit-testable (round-trip verified).
//
// Convention (matches Godot's default DIAMOND_DOWN): +col goes down-right on screen, +row down-left,
// with the map origin's diamond centred at the screen origin.
namespace maz::game {

struct IsoGrid {
    float tileWidth = 64.0f;  // full diamond width in pixels
    float tileHeight = 32.0f; // full diamond height in pixels

    IsoGrid() = default;
    IsoGrid(float w, float h) : tileWidth(w), tileHeight(h) {}

    // Pixel centre of the diamond for integer cell (col, row).
    math::vec2 tileToScreen(const math::Vector2i& cell) const {
        const float hw = tileWidth * 0.5f;
        const float hh = tileHeight * 0.5f;
        return math::vec2(static_cast<float>(cell.x - cell.y) * hw,
                          static_cast<float>(cell.x + cell.y) * hh);
    }
    // Fractional cell for an arbitrary pixel position (col, row), before rounding.
    math::vec2 screenToTileF(const math::vec2& p) const {
        const float a = p.x / (tileWidth * 0.5f);  // = col - row
        const float b = p.y / (tileHeight * 0.5f); // = col + row
        return math::vec2((a + b) * 0.5f, (b - a) * 0.5f);
    }
    // Cell whose diamond contains the pixel position (rounded).
    math::Vector2i screenToTile(const math::vec2& p) const {
        const math::vec2 f = screenToTileF(p);
        return math::Vector2i(static_cast<int>(std::lround(f.x)), static_cast<int>(std::lround(f.y)));
    }

    // The four edge-adjacent cells (the iso "compass" neighbours).
    std::vector<math::Vector2i> neighbors(const math::Vector2i& cell) const {
        return {math::Vector2i(cell.x + 1, cell.y), math::Vector2i(cell.x - 1, cell.y),
                math::Vector2i(cell.x, cell.y + 1), math::Vector2i(cell.x, cell.y - 1)};
    }
};

} // namespace maz::game
