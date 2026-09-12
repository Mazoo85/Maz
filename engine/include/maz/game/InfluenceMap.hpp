#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cstddef>
#include <vector>

// maz::game::InfluenceMap — a tactical-AI grid where "influence" spreads outward from sources and decays,
// the standard tool for spatial reasoning in strategy and shooter AI. Drop POSITIVE influence at your own
// units and NEGATIVE at enemies, then propagate: each cell blends toward its neighbours' average and loses
// a fraction each step, so influence bleeds across the map and fades with distance. Reading the result
// answers questions no single query can: where is it SAFE vs DANGEROUS (sign and magnitude), where is the
// FRONT LINE (near-zero contour between opposing armies), and which way should a unit FLEE or ADVANCE (the
// gradient points toward higher influence). This is distinct from pathfinding (a route), flow fields (a
// movement vector field toward one goal), and noise (unstructured): an influence map is a decaying
// diffusion of gameplay meaning. Deterministic, header-only, std-only. Godot ships no influence map.
namespace maz::game {

class InfluenceMap {
public:
    InfluenceMap(int width, int height)
        : m_w(width < 0 ? 0 : width),
          m_h(height < 0 ? 0 : height),
          m_cells(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), 0.0f),
          m_scratch(m_cells.size(), 0.0f) {}

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }

    float at(int x, int y) const {
        return inBounds(x, y) ? m_cells[idx(x, y)] : 0.0f;
    }
    void set(int x, int y, float v) {
        if (inBounds(x, y)) {
            m_cells[idx(x, y)] = v;
        }
    }
    // Inject influence at a cell (accumulates; positive = friendly, negative = hostile).
    void addSource(int x, int y, float strength) {
        if (inBounds(x, y)) {
            m_cells[idx(x, y)] += strength;
        }
    }

    void clear() {
        for (float& c : m_cells) {
            c = 0.0f;
        }
    }

    // One propagation step. Each cell moves a `spread` fraction (0..1) toward its 4-neighbour average, then
    // the whole map is multiplied by `decay` (0..1) so influence fades over time/distance.
    void propagate(float decay, float spread) {
        if (m_w == 0 || m_h == 0) {
            return;
        }
        const float s = spread < 0.0f ? 0.0f : (spread > 1.0f ? 1.0f : spread);
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                const float own = m_cells[idx(x, y)];
                float sum = 0.0f;
                int count = 0;
                if (x > 0) { sum += m_cells[idx(x - 1, y)]; ++count; }
                if (x + 1 < m_w) { sum += m_cells[idx(x + 1, y)]; ++count; }
                if (y > 0) { sum += m_cells[idx(x, y - 1)]; ++count; }
                if (y + 1 < m_h) { sum += m_cells[idx(x, y + 1)]; ++count; }
                const float mean = count > 0 ? sum / static_cast<float>(count) : own;
                m_scratch[idx(x, y)] = decay * (own * (1.0f - s) + mean * s);
            }
        }
        m_cells.swap(m_scratch);
    }

    // Direction of steepest ASCENT at a cell (central differences) — points toward higher influence, i.e.
    // toward safety for a friendly reading or toward the enemy for a hostile one. Zero at a flat spot.
    math::vec2 gradient(int x, int y) const {
        const float gx = at(x + 1, y) - at(x - 1, y);
        const float gy = at(x, y + 1) - at(x, y - 1);
        return math::vec2(gx * 0.5f, gy * 0.5f);
    }

    // Highest / lowest influence cell (writes coords; returns the value). Useful for "safest/most dangerous
    // spot". Ties resolve to the first in row-major order.
    float peak(int& outX, int& outY) const { return extreme(outX, outY, true); }
    float trough(int& outX, int& outY) const { return extreme(outX, outY, false); }

private:
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
    }
    float extreme(int& outX, int& outY, bool wantMax) const {
        outX = 0;
        outY = 0;
        if (m_cells.empty()) {
            return 0.0f;
        }
        float best = m_cells[0];
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                const float v = m_cells[idx(x, y)];
                if ((wantMax && v > best) || (!wantMax && v < best)) {
                    best = v;
                    outX = x;
                    outY = y;
                }
            }
        }
        return best;
    }

    int m_w;
    int m_h;
    std::vector<float> m_cells;
    std::vector<float> m_scratch;
};

} // namespace maz::game
