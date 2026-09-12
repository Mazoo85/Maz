#pragma once

#include <cstddef>
#include <vector>

// maz::game reaction-diffusion — a Gray-Scott simulation that grows organic Turing patterns (spots,
// stripes, mazes, coral, mitosis) from two diffusing/reacting chemicals U and V on a grid. It is the
// classic procedural source for animal-coat textures, rust/lichen growth, and alien-surface detail that
// plain noise can't produce, because the pattern emerges from a feedback rule rather than a static
// field. Godot has no built-in reaction-diffusion, so this is a beyond-Godot procedural utility. The
// grid is toroidal (wrap-around), the step is a standard explicit Euler update with a 5-point Laplacian,
// and everything is deterministic. Header-only, std-only.
namespace maz::game {

struct GrayScottParams {
    float feed = 0.0367f;  // F: rate U is replenished
    float kill = 0.0649f;  // K: rate V is removed
    float diffU = 0.16f;   // U diffusion rate
    float diffV = 0.08f;   // V diffusion rate
    float dt = 1.0f;       // time step

    static GrayScottParams mitosis() { return {0.0367f, 0.0649f, 0.16f, 0.08f, 1.0f}; }
    static GrayScottParams coral() { return {0.0545f, 0.062f, 0.16f, 0.08f, 1.0f}; }
};

class ReactionDiffusion {
public:
    ReactionDiffusion(int width, int height, const GrayScottParams& params = GrayScottParams::mitosis())
        : m_w(width > 0 ? width : 0), m_h(height > 0 ? height : 0), m_params(params) {
        const std::size_t n = cells();
        m_u.assign(n, 1.0f); // start fully saturated with U
        m_v.assign(n, 0.0f); // and no V
        m_uNext.assign(n, 0.0f);
        m_vNext.assign(n, 0.0f);
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    std::size_t cells() const { return static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h); }
    const std::vector<float>& u() const { return m_u; }
    const std::vector<float>& v() const { return m_v; }
    float uAt(int x, int y) const { return m_u[index(x, y)]; }
    float vAt(int x, int y) const { return m_v[index(x, y)]; }

    // Seed a filled square of "V" (radius r) centred at (cx, cy) to kick off a pattern. U is halved
    // there so the reaction has both reactants to work with.
    void seedSquare(int cx, int cy, int r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                const std::size_t i = index(wrap(cx + dx, m_w), wrap(cy + dy, m_h));
                m_u[i] = 0.5f;
                m_v[i] = 1.0f;
            }
        }
    }

    // Directly set the U and V fields at a cell (for custom initial conditions / tests).
    void set(int x, int y, float uVal, float vVal) {
        const std::size_t i = index(x, y);
        m_u[i] = uVal;
        m_v[i] = vVal;
    }

    // Advance the simulation by `iterations` explicit-Euler steps.
    void step(int iterations = 1) {
        if (m_w < 1 || m_h < 1) {
            return;
        }
        for (int it = 0; it < iterations; ++it) {
            stepOnce();
        }
    }

private:
    int m_w;
    int m_h;
    GrayScottParams m_params;
    std::vector<float> m_u;
    std::vector<float> m_v;
    std::vector<float> m_uNext;
    std::vector<float> m_vNext;

    static int wrap(int i, int n) { return ((i % n) + n) % n; }
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(wrap(y, m_h)) * static_cast<std::size_t>(m_w)
               + static_cast<std::size_t>(wrap(x, m_w));
    }

    // 5-point toroidal Laplacian of `f` at (x, y): neighbours minus 4x centre. Summed over the whole
    // grid this is exactly zero, so a pure-diffusion step conserves total mass.
    float laplacian(const std::vector<float>& f, int x, int y) const {
        const float c = f[index(x, y)];
        return f[index(x - 1, y)] + f[index(x + 1, y)] + f[index(x, y - 1)] + f[index(x, y + 1)]
               - 4.0f * c;
    }

    void stepOnce() {
        const float F = m_params.feed;
        const float K = m_params.kill;
        const float dU = m_params.diffU;
        const float dV = m_params.diffV;
        const float dt = m_params.dt;
        for (int y = 0; y < m_h; ++y) {
            for (int x = 0; x < m_w; ++x) {
                const std::size_t i = index(x, y);
                const float u = m_u[i];
                const float v = m_v[i];
                const float reaction = u * v * v;
                m_uNext[i] = u + (dU * laplacian(m_u, x, y) - reaction + F * (1.0f - u)) * dt;
                m_vNext[i] = v + (dV * laplacian(m_v, x, y) + reaction - (F + K) * v) * dt;
            }
        }
        m_u.swap(m_uNext);
        m_v.swap(m_vNext);
    }
};

} // namespace maz::game
