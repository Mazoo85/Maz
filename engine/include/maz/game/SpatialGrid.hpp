#pragma once

#include "maz/game/Collision.hpp"
#include "maz/math/Math.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace maz::game {

// Uniform spatial hash over the X/Z plane for broadphase AABB queries. Static level geometry (walls,
// blocks) is bucketed once into square cells; a query then tests only the solids sharing the query
// box's cells instead of the whole world. Y is ignored in bucketing, which suits ground-plane games
// where colliders are tall relative to the play area; the narrow-phase overlap test is still full 3D.
class SpatialGrid {
public:
    // Bucket `solids` into cells of `cellSize` world units. Keeps a copy so queries return AABBs.
    void build(const std::vector<Aabb>& solids, float cellSize) {
        m_cell = cellSize > 0.0f ? cellSize : 1.0f;
        m_solids = solids;
        m_cells.clear();
        m_stamp.assign(m_solids.size(), 0);
        m_query = 0;
        for (uint32_t i = 0; i < m_solids.size(); ++i) {
            const Aabb& s = m_solids[i];
            int x0, z0, x1, z1;
            cellRange(s, x0, z0, x1, z1);
            for (int z = z0; z <= z1; ++z) {
                for (int x = x0; x <= x1; ++x) {
                    m_cells[key(x, z)].push_back(i);
                }
            }
        }
    }

    // Fill `out` with the solids whose cells overlap `box` (deduplicated). Cleared first.
    void gather(const Aabb& box, std::vector<Aabb>& out) {
        out.clear();
        if (m_solids.empty()) {
            return;
        }
        ++m_query; // per-query stamp so each solid is emitted at most once
        int x0, z0, x1, z1;
        cellRange(box, x0, z0, x1, z1);
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                auto it = m_cells.find(key(x, z));
                if (it == m_cells.end()) {
                    continue;
                }
                for (uint32_t idx : it->second) {
                    if (m_stamp[idx] != m_query) {
                        m_stamp[idx] = m_query;
                        out.push_back(m_solids[idx]);
                    }
                }
            }
        }
    }

    float cellSize() const { return m_cell; }
    size_t cellCount() const { return m_cells.size(); }

    // Visit each occupied cell's world-space X/Z bounds (for debug visualization). `callback` gets
    // (minX, minZ, maxX, maxZ).
    template <typename Fn>
    void forEachOccupiedCell(Fn&& callback) const {
        for (const auto& kv : m_cells) {
            const int32_t cx = static_cast<int32_t>(kv.first & 0xffffffff);
            const int32_t cz = static_cast<int32_t>(kv.first >> 32);
            const float minX = static_cast<float>(cx) * m_cell;
            const float minZ = static_cast<float>(cz) * m_cell;
            callback(minX, minZ, minX + m_cell, minZ + m_cell);
        }
    }

private:
    static uint64_t key(int32_t x, int32_t z) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(z)) << 32) |
               static_cast<uint32_t>(x);
    }
    void cellRange(const Aabb& b, int& x0, int& z0, int& x1, int& z1) const {
        x0 = static_cast<int>(std::floor(b.min.x / m_cell));
        z0 = static_cast<int>(std::floor(b.min.z / m_cell));
        x1 = static_cast<int>(std::floor(b.max.x / m_cell));
        z1 = static_cast<int>(std::floor(b.max.z / m_cell));
    }

    float m_cell = 1.0f;
    std::vector<Aabb> m_solids;
    std::unordered_map<uint64_t, std::vector<uint32_t>> m_cells;
    std::vector<uint32_t> m_stamp; // per-solid last-query stamp for dedup
    uint32_t m_query = 0;
};

// slideMove against a SpatialGrid: same axis-separated resolution as the vector overload, but only
// the solids near the swept box are tested each step.
inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,
                            SpatialGrid& grid) {
    std::vector<Aabb> candidates;
    const int axes[3] = {0, 2, 1}; // x, then z, then y
    for (int i = 0; i < 3; ++i) {
        const int a = axes[i];
        pos[a] += delta[a];
        Aabb box{pos - halfExtents, pos + halfExtents};
        grid.gather(box, candidates);
        for (const Aabb& s : candidates) {
            if (!box.overlaps(s)) {
                continue;
            }
            if (delta[a] > 0.0f) {
                pos[a] = s.min[a] - halfExtents[a];
            } else if (delta[a] < 0.0f) {
                pos[a] = s.max[a] + halfExtents[a];
            }
            box = Aabb{pos - halfExtents, pos + halfExtents};
        }
    }
    return pos;
}

} // namespace maz::game
