#pragma once

#include "maz/math/Math.hpp"

#include <cstdint>
#include <unordered_map>

// maz::game GridMap — the data model behind Godot's GridMap node: a sparse 3D grid of cells, each
// holding a tile/mesh-library id plus one of the 24 orthogonal orientations. Only occupied cells are
// stored (an unordered_map keyed by packed integer coordinates), so a vast world costs only what is
// actually placed. This is the [CPU] half of GridMap — authoring, queries, bounds, and world<->cell
// mapping — everything a game or editor needs to build and reason about a voxel/tile level; the
// [GPU] half (instancing each cell's mesh from a MeshLibrary) layers on top and is deferred honestly
// until there's a display. Pure, header-only, deterministic, unit-tested.
namespace maz::game {

struct GridCell {
    int x = 0;
    int y = 0;
    int z = 0;
    bool operator==(const GridCell& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct GridItem {
    int tileId = -1;         // index into a mesh library; -1 means empty
    uint8_t orientation = 0; // 0..23, one of the orthogonal rotations (Godot convention)
};

class GridMap {
  public:
    void setCellSize(const math::vec3& s) { m_cellSize = s; }
    const math::vec3& cellSize() const { return m_cellSize; }

    // Place (or overwrite) a cell. A tileId < 0 clears it, matching Godot's set_cell_item(INVALID).
    void setCell(int x, int y, int z, int tileId, uint8_t orientation = 0) {
        const int64_t k = key(x, y, z);
        if (tileId < 0) {
            m_cells.erase(k);
        } else {
            m_cells[k] = GridItem{tileId, orientation};
        }
    }

    void clearCell(int x, int y, int z) { m_cells.erase(key(x, y, z)); }
    void clear() { m_cells.clear(); }

    bool hasCell(int x, int y, int z) const { return m_cells.count(key(x, y, z)) != 0; }

    // Tile id at a cell, or -1 if empty.
    int cellTile(int x, int y, int z) const {
        auto it = m_cells.find(key(x, y, z));
        return it == m_cells.end() ? -1 : it->second.tileId;
    }
    uint8_t cellOrientation(int x, int y, int z) const {
        auto it = m_cells.find(key(x, y, z));
        return it == m_cells.end() ? 0u : it->second.orientation;
    }

    std::size_t count() const { return m_cells.size(); }
    bool empty() const { return m_cells.empty(); }
    const std::unordered_map<int64_t, GridItem>& cells() const { return m_cells; }

    // Bounding box of occupied cells (inclusive). Returns false if the map is empty.
    bool bounds(GridCell& mn, GridCell& mx) const {
        if (m_cells.empty()) {
            return false;
        }
        bool first = true;
        for (const auto& kv : m_cells) {
            int x = 0;
            int y = 0;
            int z = 0;
            unkey(kv.first, x, y, z);
            if (first) {
                mn = mx = GridCell{x, y, z};
                first = false;
            } else {
                if (x < mn.x) mn.x = x;
                if (y < mn.y) mn.y = y;
                if (z < mn.z) mn.z = z;
                if (x > mx.x) mx.x = x;
                if (y > mx.y) mx.y = y;
                if (z > mx.z) mx.z = z;
            }
        }
        return true;
    }

    // Center of a cell in world space (cell origin + half a cell), scaled by cellSize.
    math::vec3 cellToWorld(int x, int y, int z) const {
        return math::vec3((static_cast<float>(x) + 0.5f) * m_cellSize.x,
                          (static_cast<float>(y) + 0.5f) * m_cellSize.y,
                          (static_cast<float>(z) + 0.5f) * m_cellSize.z);
    }

    // Which cell a world point falls in (floor division by cell size).
    GridCell worldToCell(const math::vec3& p) const {
        return GridCell{floorDiv(p.x, m_cellSize.x), floorDiv(p.y, m_cellSize.y),
                        floorDiv(p.z, m_cellSize.z)};
    }

    // Pack signed cell coordinates into one 64-bit key (21 bits each; range ±1,048,575).
    static int64_t key(int x, int y, int z) {
        const int64_t bx = static_cast<int64_t>(x) & 0x1FFFFF;
        const int64_t by = static_cast<int64_t>(y) & 0x1FFFFF;
        const int64_t bz = static_cast<int64_t>(z) & 0x1FFFFF;
        return bx | (by << 21) | (bz << 42);
    }

    static void unkey(int64_t k, int& x, int& y, int& z) {
        x = signExtend21(static_cast<int>(k & 0x1FFFFF));
        y = signExtend21(static_cast<int>((k >> 21) & 0x1FFFFF));
        z = signExtend21(static_cast<int>((k >> 42) & 0x1FFFFF));
    }

  private:
    static int signExtend21(int v) {
        // If the 21-bit value has its sign bit set, extend into the upper bits.
        return (v & 0x100000) ? (v | ~0x1FFFFF) : v;
    }

    static int floorDiv(float value, float size) {
        if (size == 0.0f) {
            return 0;
        }
        const float q = value / size;
        const float f = std::floor(q);
        return static_cast<int>(f);
    }

    std::unordered_map<int64_t, GridItem> m_cells;
    math::vec3 m_cellSize{1.0f, 1.0f, 1.0f};
};

} // namespace maz::game
