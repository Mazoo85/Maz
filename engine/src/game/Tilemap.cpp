#include "maz/game/Tilemap.hpp"

#include <cmath>
#include <cstddef>

namespace maz::game {

void Tilemap::resize(uint32_t width, uint32_t height, TileId fill) {
    m_width = width;
    m_height = height;
    m_tiles.assign(static_cast<size_t>(width) * height, fill);
}

bool Tilemap::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < static_cast<int>(m_width) && y < static_cast<int>(m_height);
}

TileId Tilemap::at(int x, int y) const {
    if (!inBounds(x, y)) {
        return 0;
    }
    return m_tiles[static_cast<size_t>(y) * m_width + static_cast<size_t>(x)];
}

void Tilemap::set(int x, int y, TileId t) {
    if (inBounds(x, y)) {
        m_tiles[static_cast<size_t>(y) * m_width + static_cast<size_t>(x)] = t;
    }
}

void Tilemap::setSolid(TileId id, bool solid) {
    if (id >= m_solid.size()) {
        m_solid.resize(static_cast<size_t>(id) + 1, 0);
    }
    m_solid[id] = solid ? 1u : 0u;
}

bool Tilemap::isSolidTile(int x, int y) const {
    if (!inBounds(x, y)) {
        return true;
    }
    const TileId id = at(x, y);
    return id < m_solid.size() && m_solid[id] != 0;
}

int Tilemap::worldToTileX(float wx) const {
    return static_cast<int>(std::floor(wx / m_tileSize));
}

int Tilemap::worldToTileY(float wy) const {
    return static_cast<int>(std::floor(wy / m_tileSize));
}

} // namespace maz::game
