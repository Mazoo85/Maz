#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

// maz::game::SweepPrune2D — a sweep-and-prune (SAP) broadphase for 2D AABBs. Where a uniform grid
// (game::SpatialGrid) buckets objects by cell, SAP sorts objects along an axis and sweeps a moving
// window: two boxes can only overlap if their projections onto that axis overlap, so once the sweep
// passes a box's right edge it can stop comparing against it. This is the broadphase engines reach
// for when object sizes vary a lot or the world is unbounded (no fixed cell size to tune) — it
// finds the same candidate pairs a grid would, but adapts to the data instead of a chosen
// resolution.
//
// build() takes id+AABB boxes; overlappingPairs() returns every pair whose AABBs actually overlap
// (the sweep prunes on the sort axis, then a cheap y-test confirms — so the result is exact, no
// false positives). The sort axis is chosen automatically as the one with greater spread (fewer
// sweep collisions). O(n log n) to sort + O(n + k) to report k pairs. Header-only, deterministic,
// no GPU — a drop-in broadphase for physics/trigger/query systems.
namespace maz::game {

class SweepPrune2D {
  public:
    struct Box {
        uint32_t id = 0;
        float min[2] = {0, 0};
        float max[2] = {0, 0};
    };

    static Box fromRect(uint32_t id, float x, float y, float w, float h) {
        Box b;
        b.id = id;
        b.min[0] = x;
        b.min[1] = y;
        b.max[0] = x + w;
        b.max[1] = y + h;
        return b;
    }

    void build(const std::vector<Box>& boxes) {
        m_boxes = boxes;
        m_axis = chooseAxis(m_boxes);
        // Sort by the chosen axis's min edge so the sweep advances left-to-right.
        std::sort(m_boxes.begin(), m_boxes.end(),
                  [a = m_axis](const Box& p, const Box& q) { return p.min[a] < q.min[a]; });
    }

    int axis() const { return m_axis; }
    size_t size() const { return m_boxes.size(); }

    // All pairs (idA, idB) whose AABBs overlap. Each unordered pair reported once; ids come from
    // the input boxes. Order within a pair follows the sorted sweep order, not the original ids.
    std::vector<std::pair<uint32_t, uint32_t>> overlappingPairs() const {
        std::vector<std::pair<uint32_t, uint32_t>> out;
        const int a = m_axis;
        const int o = a ^ 1; // the other axis
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            const Box& bi = m_boxes[i];
            for (size_t j = i + 1; j < m_boxes.size(); ++j) {
                const Box& bj = m_boxes[j];
                // Sweep prune: once bj starts past bi's right edge, no later box can overlap bi
                // either (they're sorted by min on this axis).
                if (bj.min[a] > bi.max[a]) {
                    break;
                }
                // bj overlaps bi on the sort axis; confirm the other axis for an exact AABB test.
                if (bi.min[o] <= bj.max[o] && bi.max[o] >= bj.min[o]) {
                    out.emplace_back(bi.id, bj.id);
                }
            }
        }
        return out;
    }

    // Convenience: does exactly one query box overlap-test against all stored boxes (linear scan;
    // use when you have a single moving probe rather than an all-pairs pass).
    std::vector<uint32_t> query(const float qmin[2], const float qmax[2]) const {
        std::vector<uint32_t> out;
        for (const Box& b : m_boxes) {
            if (b.min[0] <= qmax[0] && b.max[0] >= qmin[0] && b.min[1] <= qmax[1] &&
                b.max[1] >= qmin[1]) {
                out.push_back(b.id);
            }
        }
        return out;
    }

  private:
    static int chooseAxis(const std::vector<Box>& boxes) {
        if (boxes.empty()) {
            return 0;
        }
        // Variance of box centers per axis; sweep along the axis with the larger spread so the
        // moving window prunes the most.
        float mean[2] = {0, 0};
        for (const Box& b : boxes) {
            mean[0] += (b.min[0] + b.max[0]) * 0.5f;
            mean[1] += (b.min[1] + b.max[1]) * 0.5f;
        }
        const float inv = 1.0f / static_cast<float>(boxes.size());
        mean[0] *= inv;
        mean[1] *= inv;
        float var[2] = {0, 0};
        for (const Box& b : boxes) {
            const float cx = (b.min[0] + b.max[0]) * 0.5f - mean[0];
            const float cy = (b.min[1] + b.max[1]) * 0.5f - mean[1];
            var[0] += cx * cx;
            var[1] += cy * cy;
        }
        return var[1] > var[0] ? 1 : 0;
    }

    std::vector<Box> m_boxes;
    int m_axis = 0;
};

} // namespace maz::game
