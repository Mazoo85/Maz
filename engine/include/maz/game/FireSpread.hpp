#pragma once

#include "maz/math/Math.hpp" // math::vec2, dot, normalize

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::game fire / contagion spread — the grid cellular-automaton behind wildfires, a building burning down,
// or an infection creeping through a population, exactly the kind of emergent hazard a survival game
// (ZOMBOID) leans on. Each cell holds FUEL (how much is left to burn) and a burn STATE (unburnt → burning →
// burnt). Each tick a burning cell consumes its own fuel and radiates HEAT into its neighbours; an unburnt
// fuelled cell accumulates that heat and IGNITES once it crosses an ignition threshold — so more burning
// neighbours ignite it faster, cells with no fuel act as FIREBREAKS the fire cannot cross, and a WIND vector
// biases the heat so the front runs downwind faster than upwind. The model is fully DETERMINISTIC (heat
// accumulation, no RNG), so a lit source spreads at a predictable, unit-testable rate.
//
// This is distinct from the engine's other grid fields: InfluenceMap is a smooth two-way DIFFUSION,
// DijkstraMap an integer DISTANCE field, ReactionDiffusion a chemical Turing-pattern PDE, FloodFill an
// instantaneous region flood, and game::Spread is weapon-cone scatter — none models a self-consuming,
// fuel-limited, wind-driven advancing FRONT. Header-only, std-only. Godot ships no fire/contagion sim.
namespace maz::game {

enum class BurnState : std::uint8_t { Unburnt, Burning, Burnt };

struct FireCell {
    float fuel = 0.0f;   // remaining fuel; > 0 = flammable, 0 = inert / firebreak / burnt out
    float heat = 0.0f;   // accumulated incoming heat (toward ignition)
    BurnState state = BurnState::Unburnt;
};

struct FireParams {
    float burnRate = 1.0f;          // fuel a burning cell consumes per unit time
    float spreadRate = 1.0f;        // heat a burning cell delivers to each neighbour per unit time
    float ignitionThreshold = 1.0f; // heat an unburnt fuelled cell needs to ignite
    math::vec2 wind{0.0f, 0.0f};    // wind direction (need not be normalized; zero = no bias)
    float windStrength = 0.0f;      // how strongly wind biases heat toward downwind cells (>= 0)
    bool diagonal = false;          // 8-connected spread when true, else 4-connected
};

class FireGrid {
  public:
    FireGrid() = default;
    FireGrid(int width, int height)
        : m_w(width < 0 ? 0 : width),
          m_h(height < 0 ? 0 : height),
          m_cells(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h)),
          m_next(m_cells.size()) {}

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }

    const FireCell& at(int x, int y) const { return m_cells[idx(x, y)]; }
    FireCell& at(int x, int y) { return m_cells[idx(x, y)]; }

    void setFuel(int x, int y, float fuel) {
        if (inBounds(x, y)) m_cells[idx(x, y)].fuel = fuel < 0.0f ? 0.0f : fuel;
    }

    // Light a fuelled cell (no effect on an inert cell). Returns true if it started burning.
    bool ignite(int x, int y) {
        if (!inBounds(x, y)) return false;
        FireCell& c = m_cells[idx(x, y)];
        if (c.fuel > 0.0f && c.state != BurnState::Burnt) {
            c.state = BurnState::Burning;
            return true;
        }
        return false;
    }

    bool isBurning(int x, int y) const { return inBounds(x, y) && at(x, y).state == BurnState::Burning; }

    // Advance one tick of `dt`. Returns the number of cells burning AFTER the step.
    int step(float dt, const FireParams& p) {
        if (m_w == 0 || m_h == 0) return 0;
        static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
        static const int dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
        const int nn = p.diagonal ? 8 : 4;

        math::vec2 windN{0.0f, 0.0f};
        const float windLen = std::sqrt(p.wind.x * p.wind.x + p.wind.y * p.wind.y);
        if (windLen > 1e-9f) windN = p.wind / windLen;

        int burning = 0;
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                const FireCell& cur = m_cells[idx(x, y)];
                FireCell out = cur;

                if (cur.state == BurnState::Burning) {
                    out.fuel = cur.fuel - p.burnRate * dt;
                    if (out.fuel <= 0.0f) {
                        out.fuel = 0.0f;
                        out.state = BurnState::Burnt;
                    }
                } else if (cur.state == BurnState::Unburnt && cur.fuel > 0.0f) {
                    float incoming = 0.0f;
                    for (int i = 0; i < nn; ++i) {
                        const int nx = x + dx[i], ny = y + dy[i];
                        if (!inBounds(nx, ny)) continue;
                        if (m_cells[idx(nx, ny)].state != BurnState::Burning) continue;
                        // Spread direction is neighbour -> this cell, i.e. (-dx,-dy). Wind blowing along it
                        // (downwind) delivers extra heat.
                        float bias = 1.0f;
                        if (p.windStrength > 0.0f && windLen > 1e-9f) {
                            const float ilen = 1.0f / std::sqrt(static_cast<float>(dx[i] * dx[i] +
                                                                                   dy[i] * dy[i]));
                            const float sdx = static_cast<float>(-dx[i]) * ilen;
                            const float sdy = static_cast<float>(-dy[i]) * ilen;
                            const float align = sdx * windN.x + sdy * windN.y; // in [-1,1]
                            bias = 1.0f + p.windStrength * std::fmax(0.0f, align);
                        }
                        incoming += p.spreadRate * bias * dt;
                    }
                    out.heat = cur.heat + incoming;
                    if (out.heat >= p.ignitionThreshold) {
                        out.state = BurnState::Burning;
                    }
                }

                if (out.state == BurnState::Burning) ++burning;
                m_next[idx(x, y)] = out;
            }
        }
        m_cells.swap(m_next);
        return burning;
    }

  private:
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<FireCell> m_cells;
    std::vector<FireCell> m_next;
};

} // namespace maz::game
