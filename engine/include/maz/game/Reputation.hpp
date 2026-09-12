#pragma once

#include <cstddef>
#include <vector>

// maz::game faction reputation system — the standing meter behind "the guards now attack you on sight" and
// "the merchants give you a discount." Each faction carries a reputation value (clamped to a configurable
// range) that quests and deeds nudge up or down; that value maps through thresholds to a Standing tier —
// Hostile / Unfriendly / Neutral / Friendly / Allied — which the game reads to decide who fights, trades,
// or opens doors for the player. Thresholds are configurable so a game can tune how quickly goodwill turns
// to alliance. Godot ships no reputation/faction system — games hand-roll it every time — so this is a
// beyond-Godot gameplay utility that pairs with the quest and dialogue systems. Header-only, std-only,
// deterministic.
namespace maz::game {

enum class Standing { Hostile, Unfriendly, Neutral, Friendly, Allied };

class Reputation {
public:
    // Reputation values are clamped to [minRep, maxRep] (default a symmetric -100..100 scale).
    explicit Reputation(double minRep = -100.0, double maxRep = 100.0)
        : m_min(minRep), m_max(maxRep < minRep ? minRep : maxRep) {}

    // Set the tier cut-offs. A faction is Hostile at or below `hostile`, Unfriendly at or below
    // `unfriendly`, Friendly at or above `friendly`, Allied at or above `allied`, and Neutral between.
    void setThresholds(double hostile, double unfriendly, double friendly, double allied) {
        m_hostile = hostile;
        m_unfriendly = unfriendly;
        m_friendly = friendly;
        m_allied = allied;
    }

    // Register a faction at an initial reputation (clamped). A duplicate id resets its value.
    void addFaction(int id, double initial = 0.0) {
        Faction* f = find(id);
        if (f != nullptr) {
            f->rep = clamp(initial);
            return;
        }
        m_factions.push_back(Faction{id, clamp(initial)});
    }

    bool has(int id) const { return find(id) != nullptr; }
    std::size_t factionCount() const { return m_factions.size(); }

    // Current reputation, or 0 (neutral) for an unknown faction.
    double repOf(int id) const {
        const Faction* f = find(id);
        return f == nullptr ? 0.0 : f->rep;
    }

    // Set reputation to `value` (clamped); registers the faction if new.
    void set(int id, double value) {
        Faction* f = find(id);
        if (f == nullptr) {
            m_factions.push_back(Faction{id, clamp(value)});
            return;
        }
        f->rep = clamp(value);
    }

    // Change reputation by `delta` (clamped); registers the faction if new. Returns the new value.
    double modify(int id, double delta) {
        Faction* f = find(id);
        if (f == nullptr) {
            const double v = clamp(delta);
            m_factions.push_back(Faction{id, v});
            return v;
        }
        f->rep = clamp(f->rep + delta);
        return f->rep;
    }

    Standing standingOf(int id) const {
        const double rep = repOf(id);
        if (rep <= m_hostile) return Standing::Hostile;
        if (rep <= m_unfriendly) return Standing::Unfriendly;
        if (rep < m_friendly) return Standing::Neutral;
        if (rep < m_allied) return Standing::Friendly;
        return Standing::Allied;
    }

    bool isHostile(int id) const { return standingOf(id) == Standing::Hostile; }
    bool isAllied(int id) const { return standingOf(id) == Standing::Allied; }

    void clear() { m_factions.clear(); }

private:
    struct Faction {
        int id = -1;
        double rep = 0.0;
    };

    double clamp(double v) const { return v < m_min ? m_min : (v > m_max ? m_max : v); }

    Faction* find(int id) {
        for (Faction& f : m_factions)
            if (f.id == id) return &f;
        return nullptr;
    }
    const Faction* find(int id) const {
        for (const Faction& f : m_factions)
            if (f.id == id) return &f;
        return nullptr;
    }

    std::vector<Faction> m_factions;
    double m_min = -100.0;
    double m_max = 100.0;
    double m_hostile = -50.0;
    double m_unfriendly = -15.0;
    double m_friendly = 15.0;
    double m_allied = 50.0;
};

} // namespace maz::game
