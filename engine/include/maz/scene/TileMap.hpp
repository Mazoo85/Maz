#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

#include "maz/core/Assert.hpp"

// A 2D grid of TileId tiles (0 == empty) with one or more stacked layers — the
// Godot TileMap analog for 2D level data and procedural maps (pairs with iter37
// maz::math::PerlinNoise: threshold noise into tiles). Cells are row-major within a
// layer, layers stacked one after another. at() reads an out-of-bounds x/y as empty
// (safe neighbor sampling) but asserts an invalid layer; set()/fill() write in-bounds
// only (a bad coordinate/layer is programmer error); fillRegion clamps a partly-off-map
// rectangle to the on-map part. NOT thread-safe. Autotiling, tile metadata/atlases, and
// infinite/chunked maps are future refinements (not built here).

namespace maz::scene {

// Tile identifier: 0 == empty, any other value is an opaque tile index.
using TileId = std::uint32_t;

constexpr TileId kEmptyTile = 0;

class TileMap {
public:
    TileMap(int width, int height, int layers = 1)
        : m_w(width), m_h(height), m_layers(layers),
          m_cells(cellCount(width, height, layers), kEmptyTile) {}

    int width() const { return m_w; }
    int height() const { return m_h; }
    int layerCount() const { return m_layers; }

    bool inBounds(int x, int y) const { return x >= 0 && x < m_w && y >= 0 && y < m_h; }

    // Out-of-bounds x/y read returns empty (safe neighbor sampling); an invalid LAYER is a
    // programmer error and asserts.
    TileId at(int x, int y, int layer = 0) const {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::at: layer out of range");
        if (!inBounds(x, y)) {
            return kEmptyTile;
        }
        return m_cells[index(x, y, layer)];
    }

    // Writes must be in-bounds — an out-of-range coordinate/layer is programmer error.
    void set(int x, int y, TileId id, int layer = 0) {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::set: layer out of range");
        MAZ_ASSERT(inBounds(x, y), "TileMap::set: out of bounds");
        m_cells[index(x, y, layer)] = id;
    }

    // Set every cell of a layer to id.
    void fill(TileId id, int layer = 0) {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::fill: layer out of range");
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                m_cells[index(x, y, layer)] = id;
            }
        }
    }

    // Inclusive rectangle [x0..x1] x [y0..y1], CLAMPED to bounds so a partly-off-map region
    // fills only its on-map part. Coordinates are normalized (swapped if reversed).
    void fillRegion(int x0, int y0, int x1, int y1, TileId id, int layer = 0) {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::fillRegion: layer out of range");
        if (x0 > x1) {
            const int t = x0;
            x0 = x1;
            x1 = t;
        }
        if (y0 > y1) {
            const int t = y0;
            y0 = y1;
            y1 = t;
        }
        if (x0 < 0) { x0 = 0; }
        if (y0 < 0) { y0 = 0; }
        if (x1 > m_w - 1) { x1 = m_w - 1; }
        if (y1 > m_h - 1) { y1 = m_h - 1; }
        if (x0 > x1 || y0 > y1) {
            return;
        }
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                m_cells[index(x, y, layer)] = id;
            }
        }
    }

    // Reset ALL cells across ALL layers to empty.
    void clear() {
        for (TileId& cell : m_cells) {
            cell = kEmptyTile;
        }
    }

    std::size_t countNonEmpty(int layer = 0) const {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::countNonEmpty: layer out of range");
        std::size_t count = 0;
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                if (m_cells[index(x, y, layer)] != kEmptyTile) {
                    ++count;
                }
            }
        }
        return count;
    }

    bool empty(int layer = 0) const {
        MAZ_ASSERT(layer >= 0 && layer < m_layers, "TileMap::empty: layer out of range");
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                if (m_cells[index(x, y, layer)] != kEmptyTile) {
                    return false;
                }
            }
        }
        return true;
    }

private:
    // Validated cell count for the constructor — asserts positive dimensions BEFORE the
    // allocation, so a bad dimension trips the friendly assert rather than an obscure bad_alloc.
    static std::size_t cellCount(int width, int height, int layers) {
        MAZ_ASSERT(width > 0 && height > 0 && layers > 0, "TileMap: dimensions must be positive");
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
               static_cast<std::size_t>(layers);
    }

    // Flat index of (x, y, layer): layers stacked, row-major within a layer. Only call when
    // in bounds and the layer is valid.
    std::size_t index(int x, int y, int layer) const {
        return static_cast<std::size_t>(layer) * static_cast<std::size_t>(m_w) *
                   static_cast<std::size_t>(m_h) +
               static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
               static_cast<std::size_t>(x);
    }

    int m_w;
    int m_h;
    int m_layers;
    std::vector<TileId> m_cells;
};

} // namespace maz::scene
