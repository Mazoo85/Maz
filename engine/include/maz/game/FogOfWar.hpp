#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::game::FogOfWar — the persistent "what has this player seen?" memory for a tile map, the staple of RTS,
// strategy, and roguelike games. Every tile is in one of three states: Unseen (never revealed — drawn black),
// Explored (seen before but not currently in view — drawn dimmed, from memory), or Visible (in view right
// now — drawn fully lit, and where enemies actually show). This is DISTINCT from FieldOfView (M?, which
// computes the set of tiles a unit can see this instant); fog of war is the layer that REMEMBERS: it merges
// the current sight with the history so the map fills in permanently as you explore, while live vision comes
// and goes. The per-frame cycle is: beginFrame() demotes last frame's Visible tiles back to Explored, then
// you reveal() every tile currently in sight (or revealCircle() around each unit) — leaving unrevealed tiles
// as they were. Godot ships no fog-of-war primitive. Header-only, std-only, deterministic.
namespace maz::game {

enum class Visibility : std::uint8_t { Unseen = 0, Explored = 1, Visible = 2 };

class FogOfWar {
public:
    FogOfWar() = default;
    FogOfWar(int width, int height) { resize(width, height); }

    void resize(int width, int height) {
        m_w = width < 0 ? 0 : width;
        m_h = height < 0 ? 0 : height;
        m_state.assign(static_cast<std::size_t>(m_w) * static_cast<std::size_t>(m_h), Visibility::Unseen);
        m_visible.clear();
    }

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }

    // Start a new visibility frame: everything Visible last frame drops to Explored (remembered, not live).
    void beginFrame() {
        for (std::size_t idx : m_visible)
            if (m_state[idx] == Visibility::Visible) m_state[idx] = Visibility::Explored;
        m_visible.clear();
    }

    // Mark a tile as currently in view (implies it is also, and permanently, Explored). Out-of-bounds ignored.
    void reveal(int x, int y) {
        if (!inBounds(x, y)) return;
        const std::size_t idx = index(x, y);
        if (m_state[idx] != Visibility::Visible) {
            m_state[idx] = Visibility::Visible;
            m_visible.push_back(idx);
        }
    }

    // Reveal every tile within Euclidean `radius` of (cx, cy) — a unit's sight circle. radius < 0 does nothing.
    void revealCircle(int cx, int cy, int radius) {
        if (radius < 0) return;
        const int r2 = radius * radius;
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                const int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= r2) reveal(x, y);
            }
        }
    }

    Visibility visibility(int x, int y) const {
        return inBounds(x, y) ? m_state[index(x, y)] : Visibility::Unseen;
    }
    bool isVisible(int x, int y) const { return visibility(x, y) == Visibility::Visible; }
    bool isExplored(int x, int y) const { return visibility(x, y) != Visibility::Unseen; } // Explored OR Visible

    // Reveal the whole map as Explored memory (e.g. a bought map item) without marking it live-Visible.
    void exploreAll() {
        for (Visibility& v : m_state)
            if (v == Visibility::Unseen) v = Visibility::Explored;
    }

    void reset() {
        for (Visibility& v : m_state) v = Visibility::Unseen;
        m_visible.clear();
    }

    std::size_t visibleCount() const {
        std::size_t n = 0;
        for (Visibility v : m_state) if (v == Visibility::Visible) ++n;
        return n;
    }
    std::size_t exploredCount() const { // Explored OR Visible — everything ever seen
        std::size_t n = 0;
        for (Visibility v : m_state) if (v != Visibility::Unseen) ++n;
        return n;
    }

private:
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
    }

    int m_w = 0, m_h = 0;
    std::vector<Visibility> m_state;
    std::vector<std::size_t> m_visible; // indices currently Visible (so beginFrame demotes only those)
};

} // namespace maz::game
