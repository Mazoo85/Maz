#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>      // std::floor
#include <algorithm>  // std::remove

#include "maz/math/Geometry.hpp"  // maz::math::Aabb, maz::math::vec3
#include "maz/core/Assert.hpp"    // MAZ_ASSERT

// A sparse uniform spatial hash grid — a broadphase acceleration structure for
// culling, physics broadphase, and spatial queries (the Godot spatial-partition
// analog). An item is an (id, Aabb) pair rasterized into every integer cell its
// box overlaps; each occupied cell holds a bucket of ids. The grid is SPARSE: it
// is keyed by cell coordinate in a hash map (NOT a dense array), so negative and
// unbounded coordinates work with no preallocated volume — only occupied cells
// cost memory.
//
// Composes maz::math::Aabb. Queries are two-phase: a BROAD phase gathers the ids
// bucketed in the cells the query touches (deduping items that span several
// cells), then a NARROW phase keeps only ids whose stored Aabb actually overlaps
// the query region/point. NOT thread-safe. Quadtree/octree/BVH partitions, a
// smallest-candidate-set query optimization, and World/SceneGraph culling
// integration are future refinements (not built here).

namespace maz::spatial {

class SpatialHashGrid {
  public:
    explicit SpatialHashGrid(float cellSize) : m_cellSize(cellSize) {
        MAZ_ASSERT(cellSize > 0.0f, "SpatialHashGrid: cellSize must be > 0");
    }

    // Upsert: if id is already present, remove() it first so a re-insert relocates
    // it cleanly rather than duplicating it across stale cells. An invalid box
    // (min > max on any axis) is ignored (a no-op) and does NOT modify any existing
    // entry for that id — the invalid-box guard runs BEFORE the upsert remove(), so
    // there is nothing meaningful to rasterize and no prior placement is disturbed.
    void insert(std::uint32_t id, const maz::math::Aabb& box) {
        if (!box.isValid()) {
            return;
        }
        if (m_items.find(id) != m_items.end()) {
            remove(id);
        }
        const std::int32_t x0 = cellCoord(box.min.x);
        const std::int32_t x1 = cellCoord(box.max.x);
        const std::int32_t y0 = cellCoord(box.min.y);
        const std::int32_t y1 = cellCoord(box.max.y);
        const std::int32_t z0 = cellCoord(box.min.z);
        const std::int32_t z1 = cellCoord(box.max.z);
        for (std::int32_t cx = x0; cx <= x1; ++cx) {
            for (std::int32_t cy = y0; cy <= y1; ++cy) {
                for (std::int32_t cz = z0; cz <= z1; ++cz) {
                    m_cells[CellKey{cx, cy, cz}].push_back(id);
                }
            }
        }
        m_items[id] = box;
    }

    // Remove id from every cell its stored box rasterized into. Absent id is a safe
    // no-op returning false. Empty buckets are pruned so cellCount() stays accurate.
    bool remove(std::uint32_t id) {
        auto it = m_items.find(id);
        if (it == m_items.end()) {
            return false;
        }
        const maz::math::Aabb& box = it->second;
        const std::int32_t x0 = cellCoord(box.min.x);
        const std::int32_t x1 = cellCoord(box.max.x);
        const std::int32_t y0 = cellCoord(box.min.y);
        const std::int32_t y1 = cellCoord(box.max.y);
        const std::int32_t z0 = cellCoord(box.min.z);
        const std::int32_t z1 = cellCoord(box.max.z);
        for (std::int32_t cx = x0; cx <= x1; ++cx) {
            for (std::int32_t cy = y0; cy <= y1; ++cy) {
                for (std::int32_t cz = z0; cz <= z1; ++cz) {
                    auto cit = m_cells.find(CellKey{cx, cy, cz});
                    if (cit == m_cells.end()) {
                        continue;
                    }
                    std::vector<std::uint32_t>& bucket = cit->second;
                    bucket.erase(std::remove(bucket.begin(), bucket.end(), id), bucket.end());
                    if (bucket.empty()) {
                        m_cells.erase(cit);
                    }
                }
            }
        }
        m_items.erase(it);
        return true;
    }

    // Relocate id to a new box (remove then re-insert).
    void update(std::uint32_t id, const maz::math::Aabb& box) {
        remove(id);
        insert(id, box);
    }

    // Broad phase (cell candidates, deduped) + narrow phase (actual Aabb overlap).
    std::vector<std::uint32_t> queryRegion(const maz::math::Aabb& region) const {
        std::vector<std::uint32_t> result;
        if (!region.isValid()) {
            return result;
        }
        const std::int32_t x0 = cellCoord(region.min.x);
        const std::int32_t x1 = cellCoord(region.max.x);
        const std::int32_t y0 = cellCoord(region.min.y);
        const std::int32_t y1 = cellCoord(region.max.y);
        const std::int32_t z0 = cellCoord(region.min.z);
        const std::int32_t z1 = cellCoord(region.max.z);
        std::unordered_set<std::uint32_t> seen;  // dedup multi-cell items
        for (std::int32_t cx = x0; cx <= x1; ++cx) {
            for (std::int32_t cy = y0; cy <= y1; ++cy) {
                for (std::int32_t cz = z0; cz <= z1; ++cz) {
                    auto cit = m_cells.find(CellKey{cx, cy, cz});
                    if (cit == m_cells.end()) {
                        continue;
                    }
                    for (std::uint32_t id : cit->second) {
                        seen.insert(id);
                    }
                }
            }
        }
        for (std::uint32_t id : seen) {
            auto iit = m_items.find(id);
            if (iit != m_items.end() && region.intersects(iit->second)) {
                result.push_back(id);
            }
        }
        return result;
    }

    // A point lands in exactly one cell (no dedup needed), but still narrow-phase.
    std::vector<std::uint32_t> queryPoint(maz::math::vec3 p) const {
        std::vector<std::uint32_t> result;
        auto cit = m_cells.find(CellKey{cellCoord(p.x), cellCoord(p.y), cellCoord(p.z)});
        if (cit == m_cells.end()) {
            return result;
        }
        for (std::uint32_t id : cit->second) {
            if (m_items.at(id).contains(p)) {
                result.push_back(id);
            }
        }
        return result;
    }

    std::size_t size() const { return m_items.size(); }
    std::size_t cellCount() const { return m_cells.size(); }
    void clear() { m_cells.clear(); m_items.clear(); }

  private:
    struct CellKey {
        std::int32_t x, y, z;
        bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct CellKeyHash {
        std::size_t operator()(const CellKey& k) const {
            // Mix the three coords entirely in unsigned space (an unsigned multiply
            // cannot overflow-UB, and the reinterpret keeps it -Wconversion-clean).
            std::uint32_t h = static_cast<std::uint32_t>(k.x) * 73856093u ^
                              static_cast<std::uint32_t>(k.y) * 19349663u ^
                              static_cast<std::uint32_t>(k.z) * 83492791u;
            return static_cast<std::size_t>(h);
        }
    };

    // Map a world coordinate to its integer cell via std::floor — NOT an int cast.
    // This governs correct cell DISTRIBUTION, not query results: narrow-phase
    // re-filtering makes queries correct under any monotonic cell map, so a
    // truncating cast would still return correct query RESULTS. What std::floor buys
    // is honest spatial locality — a uniform grid across the origin, an accurate
    // cellCount(), and memory symmetry. A truncating cast rounds toward zero and
    // mis-cells negatives (e.g. -0.5 collapses into cell 0 alongside +0.5), aliasing
    // cells and inflating bucket sizes on one side of the origin — degrading the grid
    // even while narrow-phased query results stay correct.
    std::int32_t cellCoord(float w) const {
        return static_cast<std::int32_t>(std::floor(w / m_cellSize));
    }

    std::unordered_map<CellKey, std::vector<std::uint32_t>, CellKeyHash> m_cells;  // occupied cell -> id bucket
    std::unordered_map<std::uint32_t, maz::math::Aabb> m_items;  // id -> box (removal + narrow-phase)
    float m_cellSize;
};

} // namespace maz::spatial
