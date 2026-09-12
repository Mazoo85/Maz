#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// maz::game experience / leveling system — the character-progression backbone behind XP bars, "level up!"
// popups, and difficulty pacing. A LevelCurve defines how much experience each level costs; an
// ExperienceTrack is the live counter that accumulates XP and reports the current level, progress into it,
// and how much remains. Levels start at 1 and cost 0 XP; the curve's cost-to-next grows per level so the
// grind lengthens as characters advance. Costs and totals are integers (no float drift on level
// boundaries), and every cost is clamped to at least 1 so a curve can never grant infinite instant levels.
// Three curve shapes cover the common designs: linear (arithmetic growth), geometric (each level a fixed
// percentage more than the last — the classic RPG feel), and an explicit hand-authored table. Godot ships
// no leveling system — games hand-roll XP curves every time — so this is a beyond-Godot gameplay utility
// that completes the stat / inventory / loot RPG suite. Header-only, std-only, deterministic.
namespace maz::game {

class LevelCurve {
public:
    enum class Type { Linear, Geometric, Table };

    LevelCurve() = default;

    // cost(n) = base + step * (n - 1). Steady arithmetic growth.
    static LevelCurve linear(long long base, long long step, int maxLevel = 999) {
        LevelCurve c;
        c.m_type = Type::Linear;
        c.m_base = static_cast<double>(base);
        c.m_rate = static_cast<double>(step);
        c.m_maxLevel = maxLevel < 2 ? 2 : maxLevel;
        return c;
    }

    // cost(n) = base * (1 + growth)^(n - 1), rounded. `growth` 0.15 means +15% per level.
    static LevelCurve geometric(long long base, double growth, int maxLevel = 999) {
        LevelCurve c;
        c.m_type = Type::Geometric;
        c.m_base = static_cast<double>(base);
        c.m_rate = growth;
        c.m_maxLevel = maxLevel < 2 ? 2 : maxLevel;
        return c;
    }

    // Explicit costs: `costs[i]` is the XP to go from level (i+1) to (i+2). Max level is costs.size()+1.
    static LevelCurve table(std::vector<long long> costs) {
        LevelCurve c;
        c.m_type = Type::Table;
        c.m_table = std::move(costs);
        c.m_maxLevel = static_cast<int>(c.m_table.size()) + 1;
        if (c.m_maxLevel < 1) c.m_maxLevel = 1;
        return c;
    }

    int maxLevel() const { return m_maxLevel; }

    // XP required to advance FROM `level` to `level + 1`. Returns 0 at or beyond the max level (nothing
    // more to earn). Always >= 1 below the cap.
    long long costToNext(int level) const {
        if (level < 1 || level >= m_maxLevel) return 0;
        double v = 1.0;
        switch (m_type) {
            case Type::Linear:
                v = m_base + m_rate * static_cast<double>(level - 1);
                break;
            case Type::Geometric:
                v = m_base * std::pow(1.0 + m_rate, static_cast<double>(level - 1));
                break;
            case Type::Table:
                return clampCost(m_table[static_cast<std::size_t>(level - 1)]);
        }
        return clampCost(static_cast<long long>(std::llround(v)));
    }

    // Total XP required to be AT `level` (level 1 == 0). Sum of the costs below it.
    long long cumulativeToReach(int level) const {
        if (level <= 1) return 0;
        int top = level > m_maxLevel ? m_maxLevel : level;
        long long total = 0;
        for (int n = 1; n < top; ++n) total += costToNext(n);
        return total;
    }

    // Highest level reachable with `totalXp`. XP <= 0 is level 1; caps at maxLevel.
    int levelForTotalXp(long long totalXp) const {
        if (totalXp <= 0) return 1;
        long long acc = 0;
        int level = 1;
        while (level < m_maxLevel) {
            const long long c = costToNext(level);
            if (acc + c <= totalXp) {
                acc += c;
                ++level;
            } else {
                break;
            }
        }
        return level;
    }

private:
    static long long clampCost(long long v) { return v < 1 ? 1 : v; }

    Type m_type = Type::Linear;
    double m_base = 100.0;
    double m_rate = 0.0;
    std::vector<long long> m_table;
    int m_maxLevel = 999;
};

class ExperienceTrack {
public:
    ExperienceTrack() = default;
    explicit ExperienceTrack(LevelCurve curve) : m_curve(std::move(curve)) {}

    const LevelCurve& curve() const { return m_curve; }

    long long totalXp() const { return m_totalXp; }
    int level() const { return m_curve.levelForTotalXp(m_totalXp); }
    bool isMaxLevel() const { return level() >= m_curve.maxLevel(); }

    // Add XP (negative removes it; total floors at 0). Returns the change in level (positive = levels
    // gained, negative = levels lost).
    int addXp(long long amount) {
        const int before = level();
        m_totalXp += amount;
        if (m_totalXp < 0) m_totalXp = 0;
        return level() - before;
    }

    void setTotalXp(long long xp) { m_totalXp = xp < 0 ? 0 : xp; }
    void reset() { m_totalXp = 0; }

    // XP accumulated since reaching the current level.
    long long xpIntoLevel() const { return m_totalXp - m_curve.cumulativeToReach(level()); }

    // XP needed to go from the current level to the next. 0 at max level.
    long long xpForNextLevel() const { return m_curve.costToNext(level()); }

    // Fraction [0,1] of the way to the next level. 1.0 at max level.
    float progress() const {
        const long long need = xpForNextLevel();
        if (need <= 0) return 1.0f;
        return static_cast<float>(xpIntoLevel()) / static_cast<float>(need);
    }

private:
    LevelCurve m_curve;
    long long m_totalXp = 0;
};

} // namespace maz::game
