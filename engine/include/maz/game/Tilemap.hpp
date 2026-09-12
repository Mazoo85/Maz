#pragma once

#include <cstdint>
#include <vector>

namespace maz::game {

using TileId = uint16_t;

// A dense 2D grid of tile ids plus per-tile-id solidity. Deliberately simple and data-only:
// rendering and gameplay live on top (see the sandbox). Out-of-bounds tiles read as solid so
// the world has implicit walls at its edges.
class Tilemap {
public:
    void resize(uint32_t width, uint32_t height, TileId fill = 0);

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    float tileSize() const { return m_tileSize; }
    void setTileSize(float px) { m_tileSize = px; }

    bool inBounds(int x, int y) const;
    TileId at(int x, int y) const;   // 0 if out of bounds
    void set(int x, int y, TileId t);

    // Mark a tile id as solid (blocks movement). Grows the solidity table as needed.
    void setSolid(TileId id, bool solid);
    bool isSolidTile(int x, int y) const; // out-of-bounds counts as solid

    // World<->tile conversions (world origin at tile (0,0)'s top-left corner).
    int worldToTileX(float wx) const;
    int worldToTileY(float wy) const;

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    float m_tileSize = 32.0f;
    std::vector<TileId> m_tiles;
    std::vector<uint8_t> m_solid; // indexed by TileId
};

} // namespace maz::game
