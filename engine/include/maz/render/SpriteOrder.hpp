#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// maz::render::SpriteOrder — deterministic 2D draw ordering: canvas layers, per-item z-index, and
// Y-sort, the three knobs a 2D engine gives artists to control what draws in front of what. This is
// the sorting Godot's CanvasLayer.layer + Node2D.z_index + Y-Sort node provide, as a pure,
// dependency-light algorithm the sprite/polygon batcher can call each frame before flushing.
//
// Ordering (earlier = drawn first = further BACK; later = drawn on top):
//   1. layer     — coarse canvas layer (a HUD layer above the world, a background layer below it)
//   2. zIndex    — fine ordering within a layer (Godot's -4096..4096 z_index)
//   3. y-sort    — when enabled on an item, larger world-Y draws LATER (in front), so a character
//                  lower on the screen correctly occludes one higher up (top-down / 2.5D depth)
//   4. insertion — stable tie-breaker, so equal keys keep their submission order (no flicker)
//
// sortedIndices() returns the input indices in draw order without moving the caller's data;
// sort() reorders a vector of items in place. Y-sort is per-item (items with useYSort=false ignore
// their y, matching Godot where only YSort-parented nodes participate). Header-only, deterministic.
namespace maz::render {

struct DrawItem {
    uint32_t id = 0;       // caller's handle (sprite/draw-call id); opaque to the sorter
    int layer = 0;         // coarse canvas layer
    int zIndex = 0;        // fine order within a layer
    float ySort = 0.0f;    // world-Y used when useYSort is true (larger = in front)
    bool useYSort = false; // opt in to Y-sorting this item
};

// True if `a` should be drawn BEFORE `b` (further back). Total order via the insertion index tie-
// break passed as (ia, ib).
inline bool drawsBefore(const DrawItem& a, const DrawItem& b, size_t ia, size_t ib) {
    if (a.layer != b.layer) {
        return a.layer < b.layer;
    }
    if (a.zIndex != b.zIndex) {
        return a.zIndex < b.zIndex;
    }
    // Y-sort only when BOTH participate; a non-y-sorted item keeps zIndex/insertion ordering
    // against its neighbours (matches Godot: y-sorting is a property of the shared parent/layer).
    if (a.useYSort && b.useYSort && a.ySort != b.ySort) {
        return a.ySort < b.ySort;
    }
    return ia < ib; // stable
}

// Indices of `items` in back-to-front draw order (caller's data untouched).
inline std::vector<uint32_t> sortedIndices(const std::vector<DrawItem>& items) {
    std::vector<uint32_t> order(items.size());
    for (uint32_t i = 0; i < items.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&items](uint32_t a, uint32_t b) { return drawsBefore(items[a], items[b], a, b); });
    return order;
}

// Reorder `items` in place into back-to-front draw order (stable on equal keys).
inline void sort(std::vector<DrawItem>& items) {
    const std::vector<uint32_t> order = sortedIndices(items);
    std::vector<DrawItem> out;
    out.reserve(items.size());
    for (uint32_t i : order) {
        out.push_back(items[i]);
    }
    items.swap(out);
}

} // namespace maz::render
